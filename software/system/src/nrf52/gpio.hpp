#pragma once

#include "nrf52.hpp"


namespace gpio {

// ports
constexpr int P0(int index) { return index; }

constexpr int P1(int index) { return 32 + index; }

constexpr NRF_GPIO_Type *getPort(int pin) { return (NRF_GPIO_Type *) (pin < 32 ? NRF_P0_BASE : NRF_P1_BASE); }

// pin is disconnected
constexpr int DISCONNECTED = 0xffffffff;


enum class Drive {
	S0S1 = 0, // Standard '0', standard '1'
	H0S1 = 1, // High drive '0', standard '1'
	S0H1 = 2, // Standard '0', high drive '1'
	H0H1 = 3, // High drive '0', high drive '1'
	D0S1 = 4, // Disconnect '0' standard '1' (normally used for wired-or connections)
	D0H1 = 5, // Disconnect '0', high drive '1' (normally used for wired-or connections)
	S0D1 = 6, // Standard '0', disconnect '1' (normally used for wired-and connections)
	H0D1 = 7  // High drive '0', disconnect '1' (normally used for wired-and connections)
};

enum class Pull {
	DISABLED = 0,
	DOWN = 1, // Pull down on pin
	UP = 3 // Pull up on pin
};


// analog
// ------

inline void configureAnalog(int pin) {
	auto port = getPort(pin);
	auto &config = port->PIN_CNF[pin & 31];
	config = N(GPIO_PIN_CNF_DIR, Input)
		| N(GPIO_PIN_CNF_INPUT, Disconnect)
		| N(GPIO_PIN_CNF_PULL, Disabled);
}


// input
// -----

// configure input
inline void configureInput(int pin, Pull pull = Pull::DISABLED) {
	auto port = getPort(pin);
	port->PIN_CNF[pin & 31] =
		N(GPIO_PIN_CNF_INPUT, Connect)
			| V(GPIO_PIN_CNF_PULL, int(pull));
}

// for boardConfig.hpp
struct InputConfig {
	int pin; // port and plugIndex
	Pull pull;
	bool invert;
};

// add input config, pin can also be an output (used in input.cpp)
inline void addInputConfig(InputConfig const &config) {
	auto port = getPort(config.pin);
	int pos = config.pin & 31;

	// configure
	auto &PIN_CNF = port->PIN_CNF[pos];
	uint32_t c = PIN_CNF;
	c = (c & ~(GPIO_PIN_CNF_INPUT_Msk | GPIO_PIN_CNF_PULL_Msk))
		| N(GPIO_PIN_CNF_INPUT, Connect)
		| V(GPIO_PIN_CNF_PULL, int(config.pull));
	PIN_CNF = c;
}

// read input value
inline bool readInput(int pin) {
	auto port = getPort(pin);
	return (port->IN & (1 << (pin & 31))) != 0;
}


// output
// ------

// configure output
inline void configureOutput(int pin, Pull pull = Pull::DISABLED, Drive drive = Drive::S0S1) {
	auto port = getPort(pin);
	port->PIN_CNF[pin & 31] =
		N(GPIO_PIN_CNF_DIR, Output)
			| V(GPIO_PIN_CNF_DRIVE, int(drive))
			| V(GPIO_PIN_CNF_PULL, int(pull))
			| N(GPIO_PIN_CNF_INPUT, Disconnect);
}

// for boardConfig.hpp
struct OutputConfig {
	int pin; // port and plugIndex
	Pull pull;
	Drive drive;
	bool enabled;
	bool invert;
	bool initialValue;
};

// add output config, pin can also be an input (used in output.cpp)
inline void addOutputConfig(OutputConfig const &config) {
	auto port = getPort(config.pin);
	int pos = config.pin & 31;

	// set initial value
	if (config.initialValue != config.invert)
		port->OUTSET = 1 << pos;

	// configure
	auto &PIN_CNF = port->PIN_CNF[pos];
	uint32_t c = PIN_CNF;
	c = (c & ~(GPIO_PIN_CNF_DRIVE_Msk | GPIO_PIN_CNF_PULL_Msk | GPIO_PIN_CNF_DIR_Msk))
		| V(GPIO_PIN_CNF_PULL, int(config.pull))
		| V(GPIO_PIN_CNF_DRIVE, int(config.drive));
	if (config.enabled)
		c |= N(GPIO_PIN_CNF_DIR, Output);
	PIN_CNF = c;
}

// enable output
inline void enableOutput(int pin, bool enabled) {
	auto port = getPort(pin);
	uint32_t config = port->PIN_CNF[pin & 31];
	if (enabled)
		config |= N(GPIO_PIN_CNF_DIR, Output);
	else
		config &= ~GPIO_PIN_CNF_DIR_Msk;
	port->PIN_CNF[pin & 31] = config;
}

// set output value
inline void setOutput(int pin, bool value) {
	auto port = getPort(pin);
	if (value)
		port->OUTSET = 1 << (pin & 31);
	else
		port->OUTCLR = 1 << (pin & 31);
}

// toggle output value
inline void toggleOutput(int pin) {
	auto port = getPort(pin);
	uint32_t bit = 1 << (pin & 31);
	bool value = (port->OUT & bit) == 0;
	if (value)
		port->OUTSET = bit;
	else
		port->OUTCLR = bit;
}

} // namespace gpio
