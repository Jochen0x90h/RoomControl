#pragma once

#include <nrf52/gpio.hpp>
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

constexpr InputConfig INPUTS[] = {
	{PORT0 | 6, Pull::UP, true}, // poti button
	{PORT0 | 18, Pull::UP, true}, // reset/user button
};
constexpr int INPUT_COUNT = array::count(INPUTS);
constexpr int TRIGGER_COUNT = INPUT_COUNT;


// outputs
// -------

constexpr OutputConfig OUTPUTS[] = {
	{PORT0 | 23, Drive::S0S1, Pull::DISABLED, true, true, false}, // red led
	{PORT0 | 22, Drive::S0S1, Pull::DISABLED, true, true, false}, // green led
	{PORT0 | 24, Drive::S0S1, Pull::DISABLED, true, true, false}, // blue led
};
constexpr int OUTPUT_COUNT = array::count(OUTPUTS);


// poti
// ----

constexpr int POTI_A_PIN = PORT0 | 4;
constexpr int POTI_B_PIN = PORT0 | 5;


// spi
// ---

struct SpiConfig {
	enum Type {MASTER, WRITE_ONLY_MASTER};
	Type type;
	int csPin;
};

constexpr SpiConfig SPIS[] = {
	{SpiConfig::MASTER, PORT0 | 2}, // air sensor
	{SpiConfig::MASTER, PORT0 | 3}, // fe-ram
	{SpiConfig::WRITE_ONLY_MASTER, PORT0 | 3} // display
};

constexpr int SPI_SCK_PIN = PORT0 | 19;
constexpr int SPI_MOSI_PIN = PORT0 | 20;
constexpr int SPI_MISO_PIN = PORT0 | 21;
constexpr int SPI_DC_PIN = PORT0 | 21; // data/command for write-only display, can be same as MISO


// audio
// -----

constexpr int I2S_MCK_PIN = DISCONNECTED;
constexpr int I2S_SCK_PIN = PORT0 | 19;
constexpr int I2S_LRCK_PIN = PORT0 | 20;
constexpr int I2S_SDOUT_PIN = PORT0 | 21;


// bus
// ---

constexpr int BUS_TX_PIN = PORT0 | 3;
constexpr int BUS_RX_PIN = PORT0 | 2;


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 2;
constexpr int RADIO_RECEIVE_QUEUE_LENGTH = 4;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// motion detector
// ---------------

//constexpr int MOTION_DETECTOR_DELTA_SIGMA_PIN = PORT0 | 3;
//constexpr int MOTION_DETECTOR_COMPARATOR_INPUT = 0; // analog input
//constexpr int MOTION_DETECTOR_COMPARATOR_PIN = PORT0 | 2;

constexpr int MOTION_DETECTOR_PIR_PIN = PORT0 | 2;
constexpr int MOTION_DETECTOR_PIR_INPUT = 0;
constexpr int MOTION_DETECTOR_TRACKER_PIN = PORT0 | 3;
constexpr int MOTION_DETECTOR_TRACKER_INPUT = 1;


// usb
// ---

constexpr int USB_ENDPOINT_COUNT = 3;
