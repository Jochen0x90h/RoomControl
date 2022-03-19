#pragma once

#include "defs.hpp"
//#include <cstdint>


// data sheet: https://www.st.com/resource/en/datasheet/stm32f042f6.pdf
// refernece manual: https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf


// ports
constexpr int PA(int index) {return index;}
constexpr int PB(int index) {return 16 + index;}
constexpr int PC(int index) {return 32 + index;}
constexpr int PF(int index) {return 80 + index;}
constexpr GPIO_TypeDef *getPort(int pin) {return (GPIO_TypeDef *)(GPIOA_BASE + (pin >> 4) * 0x00000400UL);}


enum class Mode {
	INPUT = 0,
	OUTPUT = 1,
	ALTERNATE = 2,
	ANALOG = 3
};

enum class Drive {
	PUSH_PULL = 0,
	OPEN_DRAIN = 1
};

enum class Speed {
	LOW = 0,
	MEDIUM = 1,
	HIGH = 3
};

enum class Pull {
	DISABLED = 0,
	UP = 1, // Pull up on pin
	DOWN = 2 // Pull down on pin
};


// set mode of a gpio
inline void setMode(int pin, Mode mode) {
	auto port = getPort(pin);
	int pos2 = (pin & 15) << 1;
	port->MODER = (port->MODER & ~(3 << pos2)) | int(mode);
}


// input
// -----

// for boardConfig.hpp
struct InputConfig {
	int pin; // port and index
	Pull pull;
	bool invert;
};

// add input config, pin can also be an output (used in input.cpp)
inline void addInputConfig(InputConfig const &config) {
	auto port = getPort(config.pin);
	int pos2 = (config.pin & 15) << 1;

	// enable peripheral clock for the port
	RCC->AHBENR = RCC->AHBENR | (1 << (RCC_AHBENR_GPIOAEN_Pos + (config.pin >> 4)));
	
	// configure
	// MODER is either input (reset state) or output (addOutputConfig() already called)
	port->PUPDR = (port->PUPDR & ~(3 << pos2)) | (int(config.pull) << pos2);
}

// read input value
inline bool readInput(int pin) {
	auto port = getPort(pin);
	return (port->IDR & (1 << (pin & 15))) != 0; 
}


// output
// ------

inline void configureOutput(int pin, Pull pull = Pull::DISABLED, Speed speed = Speed::HIGH,
	Drive drive = Drive::PUSH_PULL)
{
	auto port = getPort(pin);
	int pos = pin & 15;
	int pos2 = (pin & 15) << 1;

	// enable peripheral clock for the port
	RCC->AHBENR = RCC->AHBENR | (1 << (RCC_AHBENR_GPIOAEN_Pos + (pin >> 4)));

	// set drive, speed and pull
	port->PUPDR = (port->PUPDR & ~(3 << pos2)) | (int(pull) << pos2);
	port->OSPEEDR = (port->OSPEEDR & ~(3 << pos2)) | (int(speed) << pos2);
	port->OTYPER = (port->OTYPER & ~(1 << pos)) | (int(drive) << pos);

	// set mode to alternate function
	port->MODER = (port->MODER & ~(3 << pos2)) | (int(Mode::OUTPUT) << pos2);
}

// for boardConfig.hpp
struct OutputConfig {
	int pin; // port and index
	Pull pull;
	Speed speed;
	Drive drive;
	bool enabled;
	bool invert;
	bool initialValue;
};

// add output config, pin can also be an input (used in output.cpp)
inline void addOutputConfig(OutputConfig const &config) {
	auto port = getPort(config.pin);
	int pos = config.pin & 15;
	int pos2 = (config.pin & 15) << 1;

	// enable peripheral clock for the port
	RCC->AHBENR = RCC->AHBENR | (1 << (RCC_AHBENR_GPIOAEN_Pos + (config.pin >> 4)));
	
	// set initial value
	port->BSRR = 1 << (pos + (config.initialValue != config.invert ? 0 : 16));

	// configure
	port->PUPDR = (port->PUPDR & ~(3 << pos2)) | (int(config.pull) << pos2);
	port->OSPEEDR = (port->OSPEEDR & ~(3 << pos2)) | (int(config.speed) << pos2);
	port->OTYPER = (port->OTYPER & ~(1 << pos)) | (int(config.drive) << pos);
	if (config.enabled)
		port->MODER = port->MODER | (int(Mode::OUTPUT) << pos2);
}

// set output value
inline void setOutput(int pin, bool value) {
	auto port = getPort(pin);
	int pos = pin & 15;
	port->BSRR = 1 << (pos + (value ? 0 : 16));
}

// toggle output value
inline void toggleOutput(int pin) {
	auto port = getPort(pin);
	int pos = pin & 15;
	port->BSRR = (0x00010001 << pos) & ~port->ODR;
}


// peripheral (alternate function)
// -------------------------------

struct PinFunction {
	int pin;
	int function;
};

// see STM32F042x4 STM32F042x6 datasheet
template <int PIN>
auto SPI_CS() {
	if constexpr (PIN == PA(4) || PIN == PA(15))
		return PinFunction{PIN, 0};
	else
	    return nullptr;
}

template <int PIN>
auto SPI_SCK() {
	if constexpr (PIN == PA(5) || PIN == PB(3))
		return PinFunction{PIN, 0};
	else
	    return nullptr;
}

template <int PIN>
auto SPI_MOSI() {
	if constexpr (PIN == PA(7) || PIN == PB(5))
		return PinFunction{PIN, 0};
	else
	    return nullptr;
}

template <int PIN>
auto SPI_MISO() {
	if constexpr (PIN == PA(6) || PIN == PB(4))
		return PinFunction{PIN, 0};
	else
	    return nullptr;
}

inline void configureAlternateInput(PinFunction pf, Pull pull = Pull::DISABLED) {
	auto port = getPort(pf.pin);
	int pos2 = (pf.pin & 15) << 1;
	int pos4 = (pf.pin & 7) << 2;

	// set alternate function
	auto &AFR = port->AFR[(pf.pin >> 3) & 1];
	AFR = (AFR & ~(15 << pos4)) | (pf.function << pos4);

	// set pull
	port->PUPDR = (port->PUPDR & ~(3 << pos2)) | (int(pull) << pos2);
	
	// set mode to alternate function
	port->MODER = (port->MODER & ~(3 << pos2)) | (int(Mode::ALTERNATE) << pos2);
}

inline void configureAlternateOutput(PinFunction pf, Pull pull = Pull::DISABLED, Speed speed = Speed::HIGH,
	Drive drive = Drive::PUSH_PULL)
{
	auto port = getPort(pf.pin);
	int pos = pf.pin & 15;
	int pos2 = (pf.pin & 15) << 1;
	int pos4 = (pf.pin & 7) << 2;
	
	// set alternate function
	auto &AFR = port->AFR[(pf.pin >> 3) & 1];
	AFR = (AFR & ~(15 << pos4)) | (pf.function << pos4);

	// set drive, speed and pull
	port->PUPDR = (port->PUPDR & ~(3 << pos2)) | (int(pull) << pos2);
	port->OSPEEDR = (port->OSPEEDR & ~(3 << pos2)) | (int(speed) << pos2);
	port->OTYPER = (port->OTYPER & ~(1 << pos)) | (int(drive) << pos);

	// set mode to alternate function
	port->MODER = (port->MODER & ~(3 << pos2)) | (int(Mode::ALTERNATE) << pos2);
}
