#include "FlashImpl.hpp"
#include "nrf52.hpp"



FlashImpl::FlashImpl(uint32_t baseAddress, int sectorCount, int sectorSize)
	: baseAddress(baseAddress), sectorCount(sectorCount), sectorSize(sectorSize & ~(PAGE_SIZE - 1))
{
}

Flash::Info FlashImpl::getInfo() {
	return {this->sectorCount, this->sectorSize, BLOCK_SIZE};
}

void FlashImpl::eraseSectorBlocking(int sectorIndex) {
	uint32_t address = this->baseAddress + sectorIndex * this->sectorSize;

	// set flash erase mode
	NRF_NVMC->CONFIG = N(NVMC_CONFIG_WEN, Een);

	// erase pages
	for (int i = 0; i < this->sectorSize; i += PAGE_SIZE) {
		NRF_NVMC->ERASEPAGE = address + i;

		// wait until flash is ready
		while (!NRF_NVMC->READY) {}
	}

	// set flash read mode
	NRF_NVMC->CONFIG = N(NVMC_CONFIG_WEN, Ren);
}

void FlashImpl::readBlocking(int address, int size, void *data) {
	auto dst = (uint32_t *)data;
	auto src = (const uint32_t *)(this->baseAddress + address);
	while (size >= 4) {
		// read 4 bytes
		*dst = *src;
		++dst;
		++src;
		size -= 4;
	}
	if (size > 0) {
		auto d = (uint8_t *)dst;
		auto s = (const uint8_t *)src;

		do {
			// read 1 byte
			*d = *s;
			++d;
			++s;
			--size;
		} while (size > 0);
	}
}

void FlashImpl::writeBlocking(int address, int size, void const *data) {
	// set flash write mode
	NRF_NVMC->CONFIG = N(NVMC_CONFIG_WEN, Wen);

	auto dst = (volatile uint32_t *)(this->baseAddress + address);
	auto src = (const uint32_t *)data;
	while (size >= 4) {
		// write 4 bytes
		*dst = *src;

		// data memory barrier
		__DMB();

		++dst;
		++src;
		size -= 4;

		// wait until flash is ready
		while (!NRF_NVMC->READY) {}
	}
	if (size > 0) {
		auto s = (const uint8_t *)src + size;
		uint32_t value = 0xffffffff;

		do {
			value <<= 8;

			// read 1 byte
			--s;
			value |= *s;
			--size;
		} while (size > 0);

		// write last 1-3 bytes
		*dst = value;

		// data memory barrier
		__DMB();

		// wait until flash is ready
		while (!NRF_NVMC->READY) {}
	}

	// set flash read mode
	NRF_NVMC->CONFIG = N(NVMC_CONFIG_WEN, Ren);
}
