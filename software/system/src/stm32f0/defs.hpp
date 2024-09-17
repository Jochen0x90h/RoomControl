#pragma once

#include "system/stm32f042x6.h"
//#include "system/stm32f051x8.h"



// nvic
// see NVIC_Type in system/core_cm0.h
inline void enableInterrupt(int n) {
	NVIC->ISER[n >> 5] = 1 << (n & 31);
}

inline void disableInterrupt(int n) {
	NVIC->ICER[n >> 5] = 1 << (n & 31);
}

inline bool isInterruptPending(int n) {
	return (NVIC->ISPR[n >> 5] & (1 << (n & 31))) != 0;
}

inline void triggerInterrupt(int n) {
	NVIC->ISPR[n >> 5] = 1 << (n & 31);
}

inline void clearInterrupt(int n) {
	NVIC->ICPR[n >> 5] = 1 << (n & 31);
}
