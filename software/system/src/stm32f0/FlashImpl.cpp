#include "FlashImpl.hpp"
#include "defs.hpp"


// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fmemory.html&cp=4_0_0_3_1_1&anchor=flash
// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fnvmc.html&cp=4_0_0_3_2

FlashImpl::FlashImpl(uint32_t baseAddress, int sectorCount, int sectorSize)
	: baseAddress(baseAddress), sectorCount(sectorCount), sectorSize(sectorSize & ~(PAGE_SIZE - 1))
{
}

Flash::Info FlashImpl::getInfo() {
	return {this->sectorCount, this->sectorSize, BLOCK_SIZE};
}

void FlashImpl::eraseSectorBlocking(int sectorIndex) {
	uint32_t address = this->baseAddress + sectorIndex * this->sectorSize;

	// unlock flash
	FLASH->KEYR = 0x45670123;
	FLASH->KEYR = 0xCDEF89AB;

	// erase pages
	for (int i = 0; i < this->sectorSize; i += PAGE_SIZE) {
		// set address of page to erase
		FLASH->AR = address + i;

		// start page erase
		FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;

		// wait until flash is ready
		while ((FLASH->SR & FLASH_SR_BSY) != 0) {}

		// check result
		if ((FLASH->SR & FLASH_SR_EOP) != 0) {
			// ok
			FLASH->SR = FLASH_SR_EOP;
		} else {
			// error
		}
	}

	// lock flash (and clear PER bit)
	FLASH->CR = FLASH_CR_LOCK;
}

void FlashImpl::readBlocking(int address, int size, void *data) {
	auto dst = (uint8_t *)data;
	auto src = (const uint8_t *)(this->baseAddress + address);
	while (size > 0) {
		// read byte
		*dst = *src;
		++dst;
		++src;
		--size;
	}
}

void FlashImpl::writeBlocking(int address, int size, void const *data) {
	// unlock flash
	FLASH->KEYR = 0x45670123;
	FLASH->KEYR = 0xCDEF89AB;

	// set flash write mode
	FLASH->CR = FLASH_CR_PG;

	auto dst = (volatile uint16_t *)(this->baseAddress + address);
	auto src = (const uint8_t *)data;
	while (size >= 2) {
		// avoid unaligned access
		uint16_t value = src[0] | (src[1] << 8);

		// write 2 bytes
		*dst = value;

		// data memory barrier
		__DMB();

		++dst;
		src += 2;
		size -= 2;

		// wait until flash is ready
		while ((FLASH->SR & FLASH_SR_BSY) != 0) {}

		// check result
		if ((FLASH->SR & FLASH_SR_EOP) != 0) {
			// ok
			FLASH->SR = FLASH_SR_EOP;
		} else {
			// error
		}
	}
	if (size > 0) {
		uint16_t value = 0xff00 | src[0];

		// write last byte
		*dst = value;

		// data memory barrier
		__DMB();

		// wait until flash is ready
		while ((FLASH->SR & FLASH_SR_BSY) != 0) {}

		// check result
		if ((FLASH->SR & FLASH_SR_EOP) != 0) {
			// ok
			FLASH->SR = FLASH_SR_EOP;
		} else {
			// error
		}
	}

	// lock flash (and clear PG bit)
	FLASH->CR = FLASH_CR_LOCK;
}
