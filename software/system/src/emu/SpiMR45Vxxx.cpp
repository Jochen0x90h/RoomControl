#include "SpiMR45Vxxx.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


constexpr uint8_t FERAM_WREN = 6; // write enable
constexpr uint8_t FERAM_WRDI = 4; // write disable
constexpr uint8_t FERAM_RDSR = 5; // read status register
constexpr uint8_t FERAM_WRSR = 1; // write status register
constexpr uint8_t FERAM_READ = 3; // read from memory array
constexpr uint8_t FERAM_WRITE = 2; // write to memory array
constexpr uint8_t FERAM_FSTRD = 11; // fast read from memory array
constexpr uint8_t FERAM_RDID = 0x9f; // read device id

inline int fsize(int fd) {
	struct stat stat;
	fstat(fd, &stat);
	return stat.st_size;
}

SpiMR45Vxxx::SpiMR45Vxxx(std::string const &filename, int size) : size(size) {
	this->file = open(filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	// fill with 0xff
	int fileSize = fsize(this->file);
	uint8_t buffer[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	for (int i = fileSize; i < size; i += 16) {
		pwrite(this->file, buffer, 16, i);
	}

	// add to list of handlers
	Loop::handlers.add(*this);
}

SpiMR45Vxxx::~SpiMR45Vxxx() {
	close(this->file);
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
		pread(this->file, r + 3, count, addr);
		break;
	}
	case FERAM_WRITE: {
		int addr = (w[1] << 8) | w[2];
		int count = writeCount - 3;
		assert(addr + count <= this->size);
		pwrite(this->file, w + 3, count, addr);
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
