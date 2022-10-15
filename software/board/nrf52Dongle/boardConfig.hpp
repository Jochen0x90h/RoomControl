#pragma once

#include <nrf52/gpio.hpp>
#include <nrf52/SpiMasterDevice.hpp>
#include <util.hpp>


// note: pins defined in this file are for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/


// flash
// -----

// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fnvmc.html&cp=4_0_0_3_2
constexpr int FLASH_PAGE_SIZE = 4096;
constexpr int FLASH_PAGE_COUNT = 16;
constexpr int FLASH_WRITE_ALIGN = 4;


// inputs
// ------

constexpr gpio::InputConfig INPUTS[] = {
	{gpio::P0(6), gpio::Pull::UP, true}, // poti button
	{gpio::P0(18), gpio::Pull::UP, true}, // reset/user button
};
constexpr int INPUT_COUNT = array::count(INPUTS);
constexpr int TRIGGER_COUNT = INPUT_COUNT;


// outputs
// -------

constexpr gpio::OutputConfig OUTPUTS[] = {
	{gpio::P0(23), gpio::Pull::DISABLED, gpio::Drive::S0S1, true, true, false}, // red led
	{gpio::P0(22), gpio::Pull::DISABLED, gpio::Drive::S0S1, true, true, false}, // green led
	{gpio::P0(24), gpio::Pull::DISABLED, gpio::Drive::S0S1, true, true, false}, // blue led
};
constexpr int OUTPUT_COUNT = array::count(OUTPUTS);


// poti
// ----

constexpr int POTI_A_PIN = gpio::P0(4);
constexpr int POTI_B_PIN = gpio::P0(5);


// spi
// ---
/*
struct SpiConfig {
	enum Type {MASTER, WRITE_ONLY_MASTER};

	Type type;
	int csPin;
};

constexpr SpiConfig SPI_CONTEXTS[] = {
	{SpiConfig::MASTER, gpio::P0(2)}, // air sensor
	{SpiConfig::MASTER, gpio::P0(3)}, // fe-ram
	{SpiConfig::WRITE_ONLY_MASTER, gpio::P0(3)} // display
};

constexpr int SPI_SCK_PIN = gpio::P0(19);
constexpr int SPI_MOSI_PIN = gpio::P0(20);
constexpr int SPI_MISO_PIN = gpio::P0(21);
constexpr int SPI_DC_PIN = gpio::P0(21); // data/command for write-only display, can be same as MISO
*/
struct Drivers {
	SpiMasterDevice spi{3,
		gpio::P0(19),
		gpio::P0(20),
		gpio::P0(21),
		gpio::P0(21)}; // data/command for write-only display, can be same as MISO
	SpiMasterDevice::Channel airSensor{spi, gpio::P0(2)};
	SpiMasterDevice::Channel display{spi, gpio::P0(3), true};
	SpiMasterDevice::Channel feRam{spi, gpio::P0(3)};
};


// audio
// -----

constexpr int I2S_MCK_PIN = gpio::DISCONNECTED;
constexpr int I2S_SCK_PIN = gpio::P0(19);
constexpr int I2S_LRCK_PIN = gpio::P0(20);
constexpr int I2S_SDOUT_PIN = gpio::P0(21);


// bus
// ---

constexpr int BUS_TX_PIN = gpio::P0(3);
constexpr int BUS_RX_PIN = gpio::P0(2);


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 1;
constexpr int RADIO_RECEIVE_QUEUE_LENGTH = 4;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// motion detector
// ---------------

//constexpr int MOTION_DETECTOR_DELTA_SIGMA_PIN = PORT0 | 3;
//constexpr int MOTION_DETECTOR_COMPARATOR_INPUT = 0; // analog input
//constexpr int MOTION_DETECTOR_COMPARATOR_PIN = PORT0 | 2;

constexpr int MOTION_DETECTOR_PIR_PIN = gpio::P0(2);
constexpr int MOTION_DETECTOR_PIR_INPUT = 0;
constexpr int MOTION_DETECTOR_TRACKER_PIN = gpio::P0(3);
constexpr int MOTION_DETECTOR_TRACKER_INPUT = 1;


// usb
// ---

constexpr int USB_ENDPOINT_COUNT = 3;
