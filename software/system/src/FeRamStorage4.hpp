#include <Storage.hpp>
#include <SpiMaster.hpp>


class FeRamStorage4Base : public Storage {
public:
	FeRamStorage4Base(SpiMaster &spi, int maxId, uint8_t *sequenceCounters);

	[[nodiscard]] Awaitable<ReadParameters> read(int id, int &size, void *data, Status &status) override;
	[[nodiscard]] Awaitable<WriteParameters> write(int id, int size, void const *data, Status &status) override;
	[[nodiscard]] Awaitable<ClearParameters> clear(Status &status) override;

	Status readBlocking(int id, int &size, void *data) override;
	Status writeBlocking(int id, int size, void const *data) override;
	Status clearBlocking() override;

protected:
	Coroutine reader();
	Coroutine writer();

	Status readBuffer(int &size, void *data, int index, uint8_t const *buffer);

	void setSequenceCounter(int index, int counter) {
		uint8_t &sc = this->sequenceCounters[index >> 2];
		int pos = (index & 3) * 2;
		sc = (sc & ~(3 << pos)) | (counter << pos);
	}

	int nextSequenceCounter(int index) {
		uint8_t &sc = this->sequenceCounters[index >> 2];
		int pos = (index & 3) * 2;
		int counter = ((sc >> pos) + 1) & 3;
		sc = (sc & ~(3 << pos)) | (counter << pos);
		return counter;
	}

	SpiMaster &spi;
	Waitlist<ReadParameters> readWaitlist;
	Barrier<> readBarrier;

	Waitlist<WriteParameters> writeWaitlist;
	Barrier<> writeBarrier;

	Waitlist<ClearParameters> clearWaitlist;
	Barrier<> clearBarrier;

	int maxId;
	uint8_t *sequenceCounters;
};

/**
 * Storage working on an FeRam connected via SPI. Maximum element size is 4 bytes
 * @tparam FERAM_SIZE size of FeRam in bytes
 */
template <int FERAM_SIZE>
class FeRamStorage4 : public FeRamStorage4Base {
public:
	static constexpr int MAX_ID = FERAM_SIZE / 10 - 1;

	FeRamStorage4(SpiMaster &spi) : FeRamStorage4Base(spi, MAX_ID, sequenceCountersBuffer) {}

protected:
	uint8_t sequenceCountersBuffer[(MAX_ID + 1 + 3) / 4];
};
