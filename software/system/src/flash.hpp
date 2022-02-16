#pragma once

#include <boardConfig.hpp>
#include <cstdint>


namespace flash {

/**
 * Initialize the flash
 */
#ifdef EMU
void init(char const *fileName = "flash.bin");
#else
void init();
#endif

/**
 * Get address of a flash page. Size of a flash page is FLASH_PAGE_SIZE, number of available pages is FLASH_PAGE_COUNT
 * @param pageIndex index of flash page
 */
const uint8_t *getAddress(int pageIndex);

/**
 * Check if a flash page is empty
 * @param pageIndex index of flash page
 */
inline bool isEmpty(int pageIndex) {
	const uint32_t *page = reinterpret_cast<const uint32_t*>(getAddress(pageIndex));
	for (int i = 0; i < FLASH_PAGE_SIZE >> 2; ++i) {
		if (page[i] != 0xffffffff)
			return false;
	}
	return true;
}
	
/**
 * Erase flash page
 * @param pageIndex index of flash page
 */
void erase(int pageIndex);

/**
 * Write to flash
 * @param address address where to write, must be in a flash page
 * @param length length of data, must be a multiple of FLASH_WRITE_ALIGN
 * @param data data to write
 */
void write(uint8_t const *address, int length, void const *data);

} // namespace flash
