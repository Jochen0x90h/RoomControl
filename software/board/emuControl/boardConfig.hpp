#pragma once

#include <emu/BusMasterImpl.hpp>
#include <emu/BusNodeImpl.hpp>
#include <emu/QuadratureDecoderImpl.hpp>
#include <emu/SpiBME680.hpp>
#include <emu/SpiSSD1309.hpp>
#include <emu/SpiMR45Vxxx.hpp>
#include <posix/SpiMasterImpl.hpp>
#include <posix/StorageImpl.hpp>
#include <posix/FlashImpl.hpp>
#include <posix/UsbDeviceImpl.hpp>
#include <FeRamStorage4.hpp>
#include <FlashStorage.hpp>
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
	SpiSSD1309 displayCommand{DISPLAY_WIDTH, DISPLAY_HEIGHT};
	SpiSSD1309::Data displayData{displayCommand};
	QuadratureDecoderImpl quadratureDecoder;

	BusMasterImpl busMaster;

	//StorageImpl storage{"storage.bin", 0xffff, 1024};
	FlashImpl flash{"flash.bin", 4, 32768, 4};
	FlashStorage storage{flash};

	//StorageImpl counters{"counters.bin", FERAM_SIZE / 10, 4};
	SpiMR45Vxxx feRam{"feram.bin", FERAM_SIZE};
	FeRamStorage4<FERAM_SIZE> counters{feRam};
};

struct DriversSpiMasterTest {
	SpiMasterImpl transfer{"transfer"};
	SpiMasterImpl command{"command"};
	SpiMasterImpl data{"data"};
};

struct DriversBusMasterTest {
	BusMasterImpl busMaster;
};

struct DriversBusNodeTest {
	BusNodeImpl busNode;
};

struct DriversFlashTest {
	FlashImpl flash{"flashTest.bin", 2, 4096, 4};
};

struct DriversStorageTest {
	FlashImpl flash{"storageTest.bin", 4, 32768, 4};
	FlashStorage storage{flash};
};
