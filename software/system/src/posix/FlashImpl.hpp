#pragma once

#include "../Flash.hpp"
#include "File.hpp"
#include <string>


class FlashImpl : public Flash {
public:
	/**
	 * Constructor
	 * @param sectorCount number of sectors
	 * @param sectorSize size of one sector
	 * @param blockSize size of block that has to be written at once
	 */
	FlashImpl(std::string const &filename, int sectorCount, int sectorSize, int blockSize);

	Info getInfo() override;
	void eraseSectorBlocking(int sectorIndex) override;
	void readBlocking(int address, int length, void *data) override;
	void writeBlocking(int address, int length, const void *data) override;

protected:
	File file;
	int sectorCount;
	int sectorSize;
	int blockSize;
};
