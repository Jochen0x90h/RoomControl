#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system/nrf52840.h" // comment out #include "system_nrf52840.h"
#include "system/nrf52840_bitfields.h"
#pragma GCC diagnostic pop

// construct bitfield from value
#define V(field, value) ((value) << field##_Pos)

// construct bitfield from named value
#define N(field, value) (field##_##value << field##_Pos)

// get bitfield
#define GET(reg, field) (((reg) & field##_Msk) >> field##_Pos)

// test if a bitfield has a given named value
#define TEST(reg, field, value) (((reg) & field##_Msk) == (field##_##value << field##_Pos))

// trigger a task
constexpr int TRIGGER = 1;

// indicator that an event was generated
constexpr int GENERATED = 1;




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

inline uint8_t getInterruptPriority(int n) {
	return NVIC->IP[n];
}

inline void setInterruptPriority(int n, uint8_t priority) {
	NVIC->IP[n] = priority;
}
