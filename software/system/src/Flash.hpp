#pragma once

#include <cstdint>


/**
 * Interface for flash that can be connected externally without memory mapping
 */
class Flash {
public:
	struct Info {
		// number of sectors available
		int sectorCount;

		// size of a sector, is a multiple of blockSize
		int sectorSize;

		// size of a block, the smallest unit that has to be written at once, is power of two
		int blockSize;
	};

	virtual ~Flash();

	/**
	 * Get flash info
	 * @return
	 */
	virtual Info getInfo() = 0;

	/**
	 * Erase a flash sector
	 * @param sectorIndex
	 */
	virtual void eraseSectorBlocking(int sectorIndex) = 0;

	/**
	 * Read from flash
	 * @param address address where to write, must be aligned to blockSize
	 * @param size size of data, does not need to be aligned to blockSize
	 * @param data data to read
	 */
	virtual void readBlocking(int address, int size, void *data) = 0;

	/**
	 * Write into flash
	 * @param address address where to write, must be aligned to blockSize
	 * @param size size of data, does not need to be aligned to blockSize
	 * @param data data to write
	 */
	virtual void writeBlocking(int address, int size, const void *data) = 0;
};
