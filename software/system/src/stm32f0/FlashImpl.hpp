#pragma once

#include "../Flash.hpp"


/**
 * Implementation of flash interface for stm32f0
 */
class FlashImpl : public Flash {
public:
	// size of a page that has to be erased at once
	static constexpr int PAGE_SIZE = 1024;

	// size of a block that has to be written at once
	static constexpr int BLOCK_SIZE = 2;

	/**
	 * Constructor
	 * @param baseAddress base address of flash, must be a multiple of PAGE_SIZE
	 * @param sectorCount number of sectors
	 * @param sectorSize size of one sector, must be a multiple of PAGE_SIZE
	 */
	FlashImpl(uint32_t baseAddress, int sectorCount, int sectorSize);
	Info getInfo() override;
	void eraseSectorBlocking(int sectorIndex) override;
	void readBlocking(int address, int size, void *data) override;
	void writeBlocking(int address, int size, void const *data) override;

protected:
	uint32_t baseAddress;
	int sectorCount;
	int sectorSize;
};
