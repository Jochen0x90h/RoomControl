#include "flash.hpp"
#include <algorithm>
#include <assert.h>
#include <util.hpp>
#include <fstream>


namespace flash {

static uint8_t data[FLASH_PAGE_COUNT * FLASH_PAGE_SIZE];

std::fstream file;

void init() {
//todo: emulate corrupted flash

	// erase emulated flash
	array::fill(data, 0xff);
	
	// open flash file
	flash::file.open("flash.bin", std::ios::binary);

	// read flash contents from file
	flash::file.read(reinterpret_cast<char *>(data), FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);
}

const uint8_t *getAddress(int pageIndex) {
	assert(pageIndex <= FLASH_PAGE_COUNT);
	return data + pageIndex * FLASH_PAGE_SIZE;
}

void erase(int pageIndex) {
	uint8_t *page = (uint8_t*)getAddress(pageIndex);
	std::fill(page, page + FLASH_PAGE_SIZE, 0xff);

	// write to file
	flash::file.seekg(0);
	flash::file.write(reinterpret_cast<char *>(data), FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);
	flash::file.flush();
}

void write(uint8_t const *address, int length, void const *data) {
	array::copy(length, const_cast<uint8_t*>(address), reinterpret_cast<const uint8_t*>(data));

	// write to file
	flash::file.seekg(0);
	flash::file.write(reinterpret_cast<char const *>(data), FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);
	flash::file.flush();
}

} // namespace flash
