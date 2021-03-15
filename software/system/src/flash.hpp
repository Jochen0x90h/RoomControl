#pragma once

#include <config.hpp>
#include <cstdint>


namespace flash {

const uint8_t *getAddress(int pageIndex);

inline bool isEmpty(int pageIndex) {
	const uint32_t *page = reinterpret_cast<const uint32_t*>(getAddress(pageIndex));
	for (int i = 0; i < FLASH_PAGE_SIZE >> 2; ++i) {
		if (page[i] != 0xffffffff)
			return false;
	}
	return true;
}
	
void erase(int pageIndex);
		
void write(const uint8_t *address, const void *data, int size);

} // namespace flash
