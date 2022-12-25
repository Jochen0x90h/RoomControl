#include "../Calendar.hpp"
#include "nrf52.hpp"
#include "Loop.hpp"
#include <assert.hpp>


/*
	Dependencies:
		timer
	
	Config:
	
	Resources:
		NRF_RTC0
			CC[2]
*/
namespace Calendar {

// waiting coroutines
Waitlist<> waitlist;


uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t weekday = 0;

// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_RTC0->EVENTS_COMPARE[2]) {
		// clear pending interrupt flags at peripheral and NVIC
		NRF_RTC0->EVENTS_COMPARE[2] = 0;
		clearInterrupt(RTC0_IRQn);

		NRF_RTC0->CC[2] = (NRF_RTC0->COUNTER + 16384 + 256) & ~16383;

		// increment clock time
		if (++Calendar::seconds == 60) {
			Calendar::seconds = 0;
			if (++Calendar::minutes == 60) {
				Calendar::minutes = 0;
				if (++Calendar::hours == 24) {
					Calendar::hours = 0;
					if (++Calendar::weekday == 7)
						Calendar::weekday = 0;
				}
			}
		}

		// resume all waiting coroutines
		Calendar::waitlist.resumeAll();
	}

	// call next handler in chain
	Calendar::nextHandler();
}

void init() {
	if (Calendar::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	Calendar::nextHandler = loop::addHandler(handle);

	// use channel 2 of RTC0
	NRF_RTC0->CC[2] = (NRF_RTC0->COUNTER + 16384 + 256) & ~16383;
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE2, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	NRF_RTC0->TASKS_START = TRIGGER;
}

ClockTime now() {
	return ClockTime(Calendar::weekday, Calendar::hours, Calendar::minutes, Calendar::seconds);
}

Awaitable<> secondTick() {
	return {Calendar::waitlist};
}

} // namespace Calendar
