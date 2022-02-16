#pragma once

#include <util.hpp>


// flash
// -----

constexpr int FLASH_PAGE_SIZE = 4096;
constexpr int FLASH_PAGE_COUNT = 32;
constexpr int FLASH_WRITE_ALIGN = 4;


// inputs
// ------

constexpr int INPUT_COUNT = 2;


// outputs
// -------

constexpr struct {uint32_t color; bool enabled; bool initialValue;} OUTPUTS[4] = {
	{0x0000ff, true, false}, // red led
	{0x00ff00, true, false}, // green led
	{0xff0000, true, false}, // blue led
	{0xffffff, true, false}, // heating
};
constexpr int OUTPUT_COUNT = array::count(OUTPUTS);


// spi
// ---

constexpr int SPI_CONTEXT_COUNT = 2;


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 2;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// network
// -------

constexpr int NETWORK_CONTEXT_COUNT = 2;
