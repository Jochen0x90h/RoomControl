#pragma once

#include "../Flash.hpp"


/**
 * Implementation of flash interface for stm32f0
 * See section 3 in reference manual: https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
 */
class FlashImpl : public Flash {
public:
	static constexpr int PAGE_SIZE = 1024;
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
