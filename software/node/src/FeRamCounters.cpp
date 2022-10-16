#include "FeRamCounters.hpp"
#include <util.hpp>



constexpr uint8_t FERAM_WREN = 6; // write enable
constexpr uint8_t FERAM_WRDI = 4; // write disable
constexpr uint8_t FERAM_RDSR = 5; // read status register
constexpr uint8_t FERAM_WRSR = 1; // write status register
constexpr uint8_t FERAM_READ = 3; // read from memory array
constexpr uint8_t FERAM_WRITE = 2; // write to memory array
constexpr uint8_t FERAM_FSTRD = 11; // fast read from memory array
constexpr uint8_t FERAM_RDID = 0x9f; // read device id



static uint8_t checksum(uint8_t const *buffer) {
	// layout of value: sequence counter (2 bit), checksum (3 bit), size (3 bit)
	// idea: the checksum is the sum of the inverted sequence counter and the size
	auto value = buffer[4];
	return (value & 0x38) | ((~(value >> 3) + (value << 3)) & ~0x38);
}

struct Resumer {
	Resumer(Barrier<> &barrier) : barrier(barrier) {}
	~Resumer() {this->barrier.resumeFirst();}
	Barrier<> &barrier;
};

// FeRamCounters

FeRamCountersBase::FeRamCountersBase(SpiMaster &spi, int maxElementCount, uint8_t *sequenceCounters)
	: spi(spi), maxElementCount(maxElementCount), sequenceCounters(sequenceCounters)
{
	// start coroutines
	reader();
	writer();
}

Awaitable<Storage::ReadParameters> FeRamCountersBase::read(int index, int &size, void *data, Status &status) {
	if (index >= this->maxElementCount) {
		assert(false);
		size = 0;
		status = Status::ELEMENT_COUNT_EXCEEDED;
		return {};
	}
	Resumer r(this->readBarrier);
	return {this->readWaitlist, index, &size, data, &status};
}

Awaitable<Storage::WriteParameters> FeRamCountersBase::write(int index, int size, void const *data, Status &status) {
	if (index >= this->maxElementCount) {
		assert(false);
		status = Status::ELEMENT_COUNT_EXCEEDED;
		return {};
	}
	if (size > 4) {
		assert(false);
		status = Status::ELEMENT_SIZE_EXCEEDED;
		return {};
	}
	Resumer r(this->writeBarrier);
	return {this->writeWaitlist, index, size, data, &status};
}

Awaitable<Storage::ClearParameters> FeRamCountersBase::clear(Status &status) {
	Resumer r(this->clearBarrier);
	return {this->clearWaitlist, &status};
}

Storage::Status FeRamCountersBase::readBlocking(int index, int &size, void *data) {
	if (index >= this->maxElementCount) {
		assert(false);
		size = 0;
		return Status::ELEMENT_COUNT_EXCEEDED;
	}

	// read all 10 bytes for the given id into a buffer
	int address = index * 10;
	uint8_t hi = address >> 8;
	uint8_t lo = address;
	uint8_t command[3] = {FERAM_READ, hi, lo};
	uint8_t buffer[3 + 10];
	this->spi.transferBlocking(3, command, 3 + 10, buffer);

	// read data from buffer
	return readBuffer(size, data, index, buffer + 3);
}

Storage::Status FeRamCountersBase::writeBlocking(int index, int size, void const *data) {
	if (index >= this->maxElementCount) {
		assert(false);
		return Status::ELEMENT_COUNT_EXCEEDED;
	}
	if (size > 4) {
		assert(false);
		return Status::ELEMENT_SIZE_EXCEEDED;
	}

	// write value according to lsb of sequenceCounter
	int sequenceCounter = nextSequenceCounter(index);
	int address = index * 10 + 5 * (sequenceCounter & 1);
	uint8_t hi = address >> 8;
	uint8_t lo = address;
	uint8_t c = (sequenceCounter << 6) | size;
	uint8_t buffer[3 + 5] = {FERAM_WRITE, hi, lo,0xff, 0xff, 0xff, 0xff, c};
	array::copy(size, buffer + 3, reinterpret_cast<uint8_t const *>(data));
	buffer[3 + 4] = checksum(buffer + 3);
	this->spi.transferBlocking(3 + size + 1, buffer, 0, nullptr);
	return Status::OK;
}

Storage::Status FeRamCountersBase::clearBlocking() {

	return Status::OK;
}

Coroutine FeRamCountersBase::reader() {
	while (true) {
		co_await this->readBarrier.wait();

		while (!this->readWaitlist.isEmpty()) {
			ReadParameters &p = this->readWaitlist.getFirst();

			// read all 10 bytes for the given id
			int address = p.index * 10;
			uint8_t hi = address >> 8;
			uint8_t lo = address;
			uint8_t command[3] = {FERAM_READ, hi, lo};
			uint8_t buffer[3 + 10];
			co_await this->spi.transfer(3, command, 3 + 10, buffer);

			// resume coroutine that waits for the read operation
			this->readWaitlist.resumeFirst([this, &buffer](ReadParameters &p) {
				*p.status = readBuffer(*p.size, p.data, p.index, buffer);
				return true;
			});
		}
	}
}

Coroutine FeRamCountersBase::writer() {
	while (true) {
		co_await this->writeBarrier.wait();

		while (!this->writeWaitlist.isEmpty()) {
			WriteParameters &p = this->writeWaitlist.getFirst();

			// write value according to lsb of sequenceCounter
			int sequenceCounter = nextSequenceCounter(p.index);
			int address = p.index * 10 + 5 * (sequenceCounter & 1);
			uint8_t hi = address >> 8;
			uint8_t lo = address;
			uint8_t c = (sequenceCounter << 6) | p.size;
			uint8_t buffer[3 + 5] = {FERAM_WRITE, hi, lo,0xff, 0xff, 0xff, 0xff, c};
			array::copy(p.size, buffer + 3, reinterpret_cast<uint8_t const *>(p.data));
			buffer[3 + 4] = checksum(buffer + 3);
			co_await this->spi.transfer(3 + p.size + 1, buffer, 0, nullptr);

			// resume coroutine that waits for the write operation
			this->writeWaitlist.resumeFirst([](WriteParameters &p) {
				*p.status = Status::OK;
				return true;
			});
		}
	}
}

Storage::Status FeRamCountersBase::readBuffer(int &size, void *data, int id, uint8_t const *buffer) {
	// check if a value is valid
	auto c1 = buffer[4];
	auto c2 = buffer[9];
	bool c1Valid = c1 == checksum(buffer + 0) && (c1 & 0x40) == 0;
	bool c2Valid = c2 == checksum(buffer + 5) && (c2 & 0x40) != 0;

	// check if at least one value is valid
	if (c1Valid || c2Valid) {
		int s = size;
		uint8_t const *b;
		int sequenceCounter;
		if (int8_t(c1 - c2) > 0) {
			// first is more recent
			size = c1 & 7;
			b = buffer;
			sequenceCounter = c1 >> 6;
		} else {
			// second is more recent
			size = c2 & 7;
			b = buffer + 5;
			sequenceCounter = c2 >> 6;
		}
		array::copy(min(s, size), reinterpret_cast<uint8_t *>(data), b);

		// set sequence counter
		setSequenceCounter(id, sequenceCounter);

		return Status::OK;
	} else {
		// no value is valid
		size = 0;
		return (c1 & c2) == 0xff ? Status::OK : Status::CHECKSUM_ERROR;
	}
}
