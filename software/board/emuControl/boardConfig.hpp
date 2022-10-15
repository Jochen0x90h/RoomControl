#pragma once

#include <emu/SpiBME680.hpp>
#include <emu/SpiSSD1309.hpp>
#include <posix/FileStorage.hpp>
#include <util.hpp>



// inputs
// ------

constexpr int INPUT_COUNT = 2;
#define INPUT_EMU_POTI_BUTTON 0


// outputs
// -------

constexpr struct {uint32_t color; bool enabled; bool initialValue;} OUTPUTS[] = {
	{0x0000ff, true, false}, // red led
	{0x00ff00, true, false}, // green led
	{0xff0000, true, false}, // blue led
	{0xffffff, true, false}, // heating
};
constexpr int OUTPUT_COUNT = array::count(OUTPUTS);


// spi
// ---
/*
constexpr int SPI_CONTEXT_COUNT = 3;
#define SPI_EMU_BME680 0
#define SPI_EMU_SSD1309 1
#define SPI_EMU_MR45V064B 2
*/


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 2;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// bluetooth
// ---------

constexpr int BLUETOOTH_CONTEXT_COUNT = 2;


// network
// -------

constexpr int NETWORK_CONTEXT_COUNT = 2;


// drivers
// -------

constexpr int FERAM_SIZE = 8192;
constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 64;

struct Drivers {
	SpiBME680 airSensor;
	SpiSSD1309 display{DISPLAY_WIDTH, DISPLAY_HEIGHT};
	FileStorage storage{"storage.bin", 65536, 1024};
	FileStorage counters{"counters.bin", FERAM_SIZE / 10, 4};
};
