#include "Flash.hpp"
#include <algorithm>
#include <assert.h>
#include <util.hpp>
#include <fstream>


namespace Flash {

static uint8_t data[FLASH_PAGE_COUNT * FLASH_PAGE_SIZE + 1];
static std::fstream file;

void init(char const *fileName) {
//todo: emulate corrupted flash

	// read flash contents from file and one excess byte to check size
	Flash::file.open(fileName, std::fstream::in | std::fstream::binary);
	Flash::file.read(reinterpret_cast<char *>(Flash::data), sizeof(Flash::data));
	Flash::file.clear();
	size_t size = Flash::file.tellg();
	Flash::file.close();

	// check if size is ok
	if (size != FLASH_PAGE_COUNT * FLASH_PAGE_SIZE) {
		// no: erase emulated flash
		array::fill(Flash::data, 0xff);
		Flash::file.open(fileName, std::fstream::trunc | std::fstream::in | std::fstream::out | std::fstream::binary);
		Flash::file.write(reinterpret_cast<char *>(Flash::data), FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);
		Flash::file.flush();
	} else {
		// yes: open file again
		Flash::file.open(fileName, std::fstream::in | std::fstream::out | std::fstream::binary);
		//flash::file.read(reinterpret_cast<char *>(flash::data), FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);
	}
}

const uint8_t *getAddress(int pageIndex) {
	assert(pageIndex <= FLASH_PAGE_COUNT);
	return data + pageIndex * FLASH_PAGE_SIZE;
}

void erase(int pageIndex) {
	assert(pageIndex >= 0 && pageIndex < FLASH_PAGE_COUNT);
	uint8_t *page = const_cast<uint8_t *>(getAddress(pageIndex));
	std::fill(page, page + FLASH_PAGE_SIZE, 0xff);

	// write to file
	Flash::file.seekg(pageIndex * FLASH_PAGE_SIZE);
	Flash::file.write(reinterpret_cast<char *>(page), FLASH_PAGE_SIZE);
	Flash::file.flush();
}

void write(uint8_t const *address, int length, void const *data) {
	assert(address >= Flash::data && address + length <= Flash::data + FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);
	assert((intptr_t(address) & (FLASH_WRITE_ALIGN - 1)) == 0);
	//assert((length & (FLASH_WRITE_ALIGN - 1)) == 0);
	array::copy(length, const_cast<uint8_t *>(address), reinterpret_cast<const uint8_t *>(data));

	// write to file
	size_t offset = address - Flash::data;
	Flash::file.seekg(offset);
	Flash::file.write(reinterpret_cast<char const *>(data), length);
	Flash::file.flush();
}

} // namespace Flash
