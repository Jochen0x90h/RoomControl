#include "SpiMR45Vxxx.hpp"


constexpr uint8_t FERAM_WREN = 6; // write enable
constexpr uint8_t FERAM_WRDI = 4; // write disable
constexpr uint8_t FERAM_RDSR = 5; // read status register
constexpr uint8_t FERAM_WRSR = 1; // write status register
constexpr uint8_t FERAM_READ = 3; // read from memory array
constexpr uint8_t FERAM_WRITE = 2; // write to memory array
constexpr uint8_t FERAM_FSTRD = 11; // fast read from memory array
constexpr uint8_t FERAM_RDID = 0x9f; // read device id


SpiMR45Vxxx::SpiMR45Vxxx(std::string const &filename, int size)
	: file(filename, File::Mode::READ_WRITE), size(size)
{
	// set size of emulated FeRam and initialize to 0xff if necessary
	this->file.resize(size, 0xff);

	// add to list of handlers
	loop::handlers.add(*this);
}

Awaitable <SpiMaster::Parameters> SpiMR45Vxxx::transfer(int writeCount, void const *writeData, int readCount, void *readData) {
	return {this->waitlist, nullptr, writeCount, writeData, readCount, readData};
}

void SpiMR45Vxxx::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	writeCount &= 0x7fffffff;
	auto w = reinterpret_cast<uint8_t const *>(writeData);
	auto r = reinterpret_cast<uint8_t *>(readData);

	uint8_t op = w[0];
	switch (op) {
	case FERAM_READ: {
		int addr = (w[1] << 8) | w[2];
		int count = readCount - 3;
		assert(addr + count <= this->size);
		file.read(addr, count, r + 3);
		break;
	}
	case FERAM_WRITE: {
		int addr = (w[1] << 8) | w[2];
		int count = writeCount - 3;
		assert(addr + count <= this->size);
		file.write(addr, count, w + 3);
		break;
	}
	}
}

void SpiMR45Vxxx::handle(Gui &gui) {
	this->waitlist.resumeFirst([this](Parameters &p) {
		transferBlocking(p.writeCount, p.writeData, p.readCount, p.readData);
		return true;
	});
}
