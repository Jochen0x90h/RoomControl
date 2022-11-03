#pragma once

#include <emu/BusMasterImpl.hpp>
#include <emu/BusNodeImpl.hpp>
#include <emu/QuadratureDecoderImpl.hpp>
#include <emu/SpiBME680.hpp>
#include <emu/SpiSSD1309.hpp>
#include <emu/SpiMPQ6526.hpp>
#include <posix/StorageImpl.hpp>
#include <util.hpp>



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

//constexpr int SPI_CONTEXT_COUNT = 1;
//#define SPI_EMU_MPQ6526 0
constexpr int MPQ6526_MAPPING[] = {0, 1, 2, 3, 4, 5};


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 1;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// bluetooth
// ---------

constexpr int BLUETOOTH_CONTEXT_COUNT = 2;


// network
// -------

constexpr int NETWORK_CONTEXT_COUNT = 1;


// drivers
// -------

constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 64;

struct Drivers {
	SpiBME680 airSensor;
	SpiSSD1309 display{DISPLAY_WIDTH, DISPLAY_HEIGHT};
	QuadratureDecoderImpl quadratureDecoder;
	BusMasterImpl busMaster;
	BusNodeImpl busNode;
	StorageImpl storage{"storage.bin", 0xffff, 1024};
	StorageImpl counters{"counters.bin", 0xff, 4};
};

struct SwitchDrivers {
	BusNodeImpl busNode;
	SpiMPQ6526 relayDriver;
	StorageImpl storage{"storage.bin", 0xffff, 1024};
	StorageImpl counters{"counters.bin", 0xff, 4};
};
