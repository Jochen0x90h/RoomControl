#pragma once

#include <util.hpp>


// flash
// -----

constexpr int FLASH_PAGE_SIZE = 4096;
constexpr int FLASH_PAGE_COUNT = 32;
constexpr int FLASH_WRITE_ALIGN = 4;


// inputs
// ------

constexpr int INPUT_COUNT = 4;
#define INPUT_EMU_DOUBLE_ROCKER


// outputs
// -------

constexpr struct {uint32_t color; bool enabled; bool initialValue;} OUTPUTS[] = {
	{0xffffff, true, false}, // A led
	{0xffffff, true, false}, // B led
};
constexpr int OUTPUT_COUNT = array::count(OUTPUTS);


// spi
// ---

constexpr int SPI_CONTEXT_COUNT = 1;
#define SPI_EMU_MPQ6526 0
constexpr int MPQ6526_MAPPING[] = {0, 1, 2, 3, 4, 5};


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 1;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// network
// -------

constexpr int NETWORK_CONTEXT_COUNT = 1;


// storage
// -------

constexpr int STORAGE_CONTEXT_COUNT = 1;
