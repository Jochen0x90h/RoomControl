#pragma once

#include "../Flash.hpp"


class FlashImpl : public Flash {
public:
	static constexpr int PAGE_SIZE = 4096;
	static constexpr int BLOCK_SIZE = 4;

	/**
	 * Implementation of flash interface for nrf52
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
