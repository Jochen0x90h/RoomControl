#include "FlashImpl.hpp"
#include <assert.hpp>


FlashImpl::FlashImpl(std::string const &filename, int sectorCount, int sectorSize, int blockSize)
	: file(filename, File::Mode::READ_WRITE), sectorCount(sectorCount), sectorSize(sectorSize), blockSize(blockSize)
{
	int size = sectorCount * sectorSize;

	// set size of emulated flash and initialize to 0xff if necessary
	this->file.resize(size, 0xff);
}

Flash::Info FlashImpl::getInfo() {
	return {this->sectorCount, this->sectorSize, this->blockSize};
}

void FlashImpl::eraseSectorBlocking(int sectorIndex) {
	// check range
	assert(sectorIndex >= 0 && sectorIndex < this->sectorCount);

	// erase sector
	this->file.fill(sectorIndex * this->sectorSize, this->sectorSize, 0xff);
}

void FlashImpl::readBlocking(int address, int length, void *data) {
	// check block alignment
	assert(address % this->blockSize == 0);

	// check range
	assert(address >= 0 && address + length <= this->sectorCount * this->sectorSize);

	// read from file
	this->file.read(address, length, data);
}

void FlashImpl::writeBlocking(int address, int length, const void *data) {
	// check block alignment
	assert(address % this->blockSize == 0);

	// check range
	assert(address >= 0 && address + length <= this->sectorCount * this->sectorSize);

	// write to file
	this->file.write(address, length, data);
}
