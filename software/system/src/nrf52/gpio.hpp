#pragma once

#include "defs.hpp"


// io
/*
// configure a gpio as input with pull up
inline void configureInputWithPullUp(int pin) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	p->PIN_CNF[pin & 31] = N(GPIO_PIN_CNF_DIR, Input)
		| N(GPIO_PIN_CNF_INPUT, Connect)
		| N(GPIO_PIN_CNF_PULL, Pullup);
}

// configure a gpio as input with pull down
inline void configureInputWithPullDown(int pin) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	p->PIN_CNF[pin & 31] = N(GPIO_PIN_CNF_DIR, Input)
		| N(GPIO_PIN_CNF_INPUT, Connect)
		| N(GPIO_PIN_CNF_PULL, Pulldown);
}



// configure a gpio as output
inline void configureOutput(int pin) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	p->PIN_CNF[pin & 31] = N(GPIO_PIN_CNF_DIR, Output)
		| N(GPIO_PIN_CNF_INPUT, Connect)
		| N(GPIO_PIN_CNF_PULL, Disabled)
		| N(GPIO_PIN_CNF_DRIVE, S0S1);
}
*/
/*
// configure gpio drive for use by a peripheral an connected to an analog signal, therefore disconnect input
inline void configureDrive(int pin, Drive drive = Drive::S0S1) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	p->PIN_CNF[pin & 31] = N(GPIO_PIN_CNF_DIR, Input)
		| N(GPIO_PIN_CNF_INPUT, Disconnect)
		| N(GPIO_PIN_CNF_PULL, Disabled)
		| V(GPIO_PIN_CNF_DRIVE, int(drive));
}
*/



// ports
constexpr int PORT0 = 0;
constexpr int PORT1 = 32;

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
	H0D1 = 7, // High drive '0', disconnect '1' (normally used for wired-and connections)
};

enum class Pull {
	DISABLED = 0, // No pull
	DOWN = 1, // Pull down on pin
	UP = 3, // Pull up on pin
};


// analog
// ------

inline void configureAnalog(int pin) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	auto &config = port->PIN_CNF[pin & 31];
	config = N(GPIO_PIN_CNF_DIR, Input)
		| N(GPIO_PIN_CNF_INPUT, Disconnect)
		| N(GPIO_PIN_CNF_PULL, Disabled);
}


// input
// -----

// configure input
inline void configureInput(int pin, Pull pull = Pull::DISABLED) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	port->PIN_CNF[pin & 31] =
		N(GPIO_PIN_CNF_INPUT, Connect)
		| V(GPIO_PIN_CNF_PULL, int(pull));
}

// for sysConfig.hpp
struct InputConfig {
	int pin;
	Pull pull;
	bool invert;
};

// add input config, pin can also be an output (used in input.cpp)
inline void addInputConfig(int pin, Pull pull) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	auto &config = port->PIN_CNF[pin & 31];
	uint32_t c = config;
	c = (c & ~GPIO_PIN_CNF_INPUT_Msk) | N(GPIO_PIN_CNF_INPUT, Connect);
	if (pull != Pull::DISABLED)
		c = (c & ~GPIO_PIN_CNF_PULL_Msk) | V(GPIO_PIN_CNF_PULL, int(pull));
	config = c;
}

// read input value
inline bool readInput(int pin) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	return (port->IN & (1 << (pin & 31))) != 0; 
}


// output
// ------

// configure output
inline void configureOutput(int pin, Drive drive = Drive::S0S1, Pull pull = Pull::DISABLED) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	port->PIN_CNF[pin & 31] =
		V(GPIO_PIN_CNF_DRIVE, int(drive))
		| V(GPIO_PIN_CNF_PULL, int(pull))
		| N(GPIO_PIN_CNF_DIR, Output);
}

// for sysConfig.hpp
struct OutputConfig {
	int pin;
	Drive drive;
	Pull pull;
	bool enabled;
	bool invert;
	bool initialValue;
};

// add output config, pin can also be an input (used in output.cpp)
inline void addOutputConfig(int pin, Drive drive, Pull pull, bool enabled) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	auto &config = port->PIN_CNF[pin & 31];
	uint32_t c = config;
	c |= V(GPIO_PIN_CNF_DRIVE, int(drive));
	if (pull != Pull::DISABLED)
		c = (c & ~GPIO_PIN_CNF_PULL_Msk) | V(GPIO_PIN_CNF_PULL, int(pull));
	if (enabled)
		c |= N(GPIO_PIN_CNF_DIR, Output);
	config = c;
}

// enable output
inline void enableOutput(int pin, bool enabled) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	auto &config = port->PIN_CNF[pin & 31];
	if (enabled)
		config |= N(GPIO_PIN_CNF_DIR, Output);
	else
		config &= ~GPIO_PIN_CNF_DIR_Msk;
}

// set output value
inline void setOutput(int pin, bool value) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	if (value)
		port->OUTSET = 1 << (pin & 31);
	else
		port->OUTCLR = 1 << (pin & 31);
}

// toggle output value
inline void toggleOutput(int pin) {
	auto port = pin >= PORT1 ? NRF_P1 : NRF_P0;
	uint32_t bit = 1 << (pin & 31);
	bool value = (port->OUT & bit) == 0;
	if (value)
		port->OUTSET = bit;
	else
		port->OUTCLR = bit;
}

