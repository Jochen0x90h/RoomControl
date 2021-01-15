#pragma once

#include "config.hpp"
#include <stdint.h>


class Flash {
public:
	static const uint8_t *getAddress(uint8_t pageIndex);

	static bool isEmpty(uint8_t pageIndex) {
		const uint32_t *page = reinterpret_cast<const uint32_t*>(getAddress(pageIndex));
		for (int i = 0; i < FLASH_PAGE_SIZE >> 2; ++i) {
			if (page[i] != 0xffffffff)
				return false;
		}
		return true;
	}
	
	static void erase(uint8_t pageIndex);
		
	static const void write(const uint8_t *address, const void *data, int size);
};
