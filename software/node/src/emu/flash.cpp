#include "flash.hpp"
#include <algorithm>
#include <assert.h>


namespace flash {

static uint8_t data[FLASH_PAGE_COUNT * FLASH_PAGE_SIZE];

const uint8_t *getAddress(int pageIndex) {
	assert(pageIndex <= FLASH_PAGE_COUNT);
	return data + pageIndex * FLASH_PAGE_SIZE;
}

void erase(int pageIndex) {
	uint8_t *page = (uint8_t*)getAddress(pageIndex);
	std::fill(page, page + FLASH_PAGE_SIZE, 0xff);
}

void write(const uint8_t *address, const void *data, int size) {
	for (int i = 0; i < size; ++i) {
		const_cast<uint8_t*>(address)[i] = reinterpret_cast<const uint8_t*>(data)[i];
	}
}

} // namespace flash
