#include "system/nrf.h"
#include <sysConfig.hpp>


// construct bitfield from value
#define V(field, value) ((value) << field##_Pos)

// construct bitfield from named value
#define N(field, value) (field##_##value << field##_Pos)

// get bitfield
#define GET(reg, field) (((reg) & field##_Msk) >> field##_Pos)

// test if a bitfield has a given named value
#define TEST(reg, field, value) (((reg) & field##_Msk) == (field##_##value << field##_Pos))

// all tasks have the same trigger value
constexpr int Trigger = 1;

// all events have the same generated value
constexpr int Generated = 1;

// all ports have the same disconnected value
constexpr int Disconnected = 0xffffffff;


// wait for event
// see http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dai0321a/BIHICBGB.html
inline void waitForEvent() {
	
	// data synchronization barrier
	__DSB();
	
	// wait for event (interrupts trigger an event due to SEVONPEND)
	__WFE();
}


// nvic
// see NVIC_Type in system/core_cm4.h
inline void enableInterrupt(int n) {
	NVIC->ISER[n >> 5] = 1 << (n & 31);
}
inline void disableInterrupt(int n) {
	NVIC->ICER[n >> 5] = 1 << (n & 31);
}
inline bool isInterruptPending(int n) {
	return (NVIC->ISPR[n >> 5] & (1 << (n & 31))) != 0;
}
inline void clearInterrupt(int n) {
	NVIC->ICPR[n >> 5] = 1 << (n & 31);
}


// io

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

// configure a gpio as output
inline void configureOutput(int pin) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	p->PIN_CNF[pin & 31] = N(GPIO_PIN_CNF_DIR, Output)
		| N(GPIO_PIN_CNF_INPUT, Connect)
		| N(GPIO_PIN_CNF_PULL, Disabled)
		| N(GPIO_PIN_CNF_DRIVE, S0S1);
}

// configure gpio drive for use by a peripheral an connected to an analog signal, therefore disconnect input
inline void configureDrive(int pin, Drive drive = Drive::S0S1) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	p->PIN_CNF[pin & 31] = N(GPIO_PIN_CNF_DIR, Input)
		| N(GPIO_PIN_CNF_INPUT, Disconnect)
		| N(GPIO_PIN_CNF_PULL, Disabled)
		| V(GPIO_PIN_CNF_DRIVE, int(drive));
}

// set output value
inline void setOutput(int pin, bool value) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	if (value)
		p->OUTSET = 1 << (pin & 31);
	else
		p->OUTCLR = 1 << (pin & 31);
}

// toggle output value
inline void toggleOutput(int pin) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	uint32_t bit = 1 << (pin & 31);
	bool value = (p->OUT & bit) == 0;
	if (value)
		p->OUTSET = bit;
	else
		p->OUTCLR = bit;
}

// get input value
inline bool getInput(int pin) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	return (p->IN & (1 << (pin & 31))) != 0; 
}
