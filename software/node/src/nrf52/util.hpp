#include "system/nrf.h"
#include <config.hpp>


// construct bitfield from value
#define V(field, value) ((value) << field##_Pos)

// construct bitfield from named value
#define N(field, value) (field##_##value << field##_Pos)

// get bitfield
#define G(reg, field) (((reg) & field##_Msk) >> field##_Pos)

// test condition
#define T(reg, field, value) (((reg) & field##_Msk) == field##_##value << field##_Pos)

// all task triggers have the value 1
constexpr int Trigger = 1;

// all events have the value 1
constexpr int Generated = 1;


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

// configure a gpio as output
inline void configureOutput(int pin) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	p->PIN_CNF[pin & 31] = N(GPIO_PIN_CNF_DIR, Output)
		| N(GPIO_PIN_CNF_INPUT, Disconnect)
		| N(GPIO_PIN_CNF_PULL, Disabled)
		| N(GPIO_PIN_CNF_DRIVE, S0S1);
}

// set output value
inline void setOutput(int pin, bool value) {
	auto p = pin >= PORT1 ? NRF_P1 : NRF_P0;
	if (value)
		p->OUTSET = 1 << (pin & 31);
	else
		p->OUTCLR = 1 << (pin & 31);
}

inline void nop() {
}
