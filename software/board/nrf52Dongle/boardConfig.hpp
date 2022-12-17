#pragma once

#include <nrf52/gpio.hpp>
#include <nrf52/BusMasterImpl.hpp>
#include <nrf52/QuadratureDecoderImpl.hpp>
#include <nrf52/SpiMasterImpl.hpp>
#include <nrf52/FlashImpl.hpp>
#include <FlashStorage.hpp>
#include <util.hpp>


// note: pins defined in this file are for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/


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


// audio
// -----

constexpr int I2S_MCK_PIN = gpio::DISCONNECTED;
constexpr int I2S_SCK_PIN = gpio::P0(19);
constexpr int I2S_LRCK_PIN = gpio::P0(20);
constexpr int I2S_SDOUT_PIN = gpio::P0(21);


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


// drivers
// -------

struct Drivers {
	SpiMasterImpl spiDevice{
		gpio::P0(19),
		gpio::P0(20),
		gpio::P0(21),
		gpio::P0(21)}; // data/command for write-only display, can be same as MISO
	SpiMasterImpl::Channel airSensor{spiDevice, gpio::P0(2)};
	SpiMasterImpl::Channel displayCommand{spiDevice, gpio::P0(3), SpiMasterImpl::Channel::Mode::COMMAND};
	SpiMasterImpl::Channel displayData{spiDevice, gpio::P0(3), SpiMasterImpl::Channel::Mode::DATA};
	SpiMasterImpl::Channel feRam{spiDevice, gpio::P0(3)};
	QuadratureDecoderImpl quadratureDecoder{gpio::P0(4), gpio::P0(5)};

	//BusMasterImpl busMaster{gpio::P0(2), gpio::P0(3)};

	//FlashImpl flash{0xe0000 - 0x20000, 4, 32768};
	//FlashStorage storage{flash};
};

struct DriversSpiMasterTest {
	SpiMasterImpl spi{
		gpio::P0(19), // SCK
		gpio::P0(20), // MOSI
		gpio::P0(21), // MISO
		gpio::P0(21)}; // DC (data/command for write-only display, can be same as MISO)
	SpiMasterImpl::Channel transfer{spi, gpio::P0(2)};
	SpiMasterImpl::Channel command{spi, gpio::P0(3), SpiMasterImpl::Channel::Mode::COMMAND};
	SpiMasterImpl::Channel data{spi, gpio::P0(3), SpiMasterImpl::Channel::Mode::DATA};
};

struct DriversBusMasterTest {
	BusMasterImpl busMaster{gpio::P0(2), gpio::P0(3)};;
};

struct DriversFlashTest {
	FlashImpl flash{0xe0000 - 0x40000, 2, 4096};
};

struct DriversStorageTest {
	FlashImpl flash{0xe0000 - 0x40000, 4, 32768};
	FlashStorage storage{flash};
};
