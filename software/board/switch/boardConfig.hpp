#pragma once

#include <stm32f0/gpio.hpp>
#include <stm32f0/BusNodeImpl.hpp>
#include <stm32f0/SpiMasterImpl.hpp>
#include <nrf52/FlashImpl.hpp>
#include <FlashStorage.hpp>
#include <util.hpp>


// clock
// -----

constexpr int CLOCK = 8000000;


// inputs
// ------

constexpr gpio::InputConfig INPUTS[] = {
	{gpio::PA(2), gpio::Pull::UP, true}, // dummy
	{gpio::PA(3), gpio::Pull::UP, true}, // button
};
constexpr int INPUT_COUNT = array::count(INPUTS);
constexpr int TRIGGER_COUNT = INPUT_COUNT;


// outputs
// -------

// https://github.com/candle-usb/candleLight_fw/blob/master/include/config.h
constexpr gpio::OutputConfig OUTPUTS[] = {
	{gpio::PB(2), gpio::Pull::DISABLED, gpio::Speed::LOW, gpio::Drive::PUSH_PULL, true, false, false}, // dummy
	{gpio::PB(0), gpio::Pull::DISABLED, gpio::Speed::LOW, gpio::Drive::PUSH_PULL, true, false, false}, // green led
	{gpio::PB(1), gpio::Pull::DISABLED, gpio::Speed::LOW, gpio::Drive::PUSH_PULL, true, false, false}, // blue led
};
constexpr int OUTPUT_COUNT = array::count(OUTPUTS);



constexpr int MPQ6526_MAPPING[] = {0, 1, 2, 3, 4, 5};


// drivers
// -------

struct Drivers {
	SpiMasterImpl spi{1,
		gpio::PA(5),
		gpio::PA(7),
		gpio::PA(6)};
	SpiMasterImpl::Channel airSensor{spi, gpio::PA(3)};
	SpiMasterImpl::Channel display{spi, gpio::PA(4)};
};

struct SwitchDrivers {
	SpiMasterImpl spi{1,
		gpio::PA(5),
		gpio::PA(7),
		gpio::PA(6)};
	SpiMasterImpl::Channel relayDriver{spi, gpio::PA(3)};
	BusNodeImpl busNode;
};

struct DriversFlashTest {
	FlashImpl flash{0xe0000 - 0x40000, 2, 4096};
};

struct DriversStorageTest {
	FlashImpl flash{0xe0000 - 0x40000, 4, 32768};
	FlashStorage storage{flash};
};
