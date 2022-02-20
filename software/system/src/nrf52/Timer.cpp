#include "../Timer.hpp"
#include "Loop.hpp"
#include "defs.hpp"
#include <assert.hpp>


/*
	https://infocenter.nordicsemi.com/topic/struct_nrf52/struct/nrf52840.html

	Dependencies:

	Config:

	Resources:
		NRF_RTC0
			CC[0]
*/
namespace Timer {

SystemTime systemTime = {};

// next timeout of a timer in the list
SystemTime next;

// waiting coroutines
Waitlist<SystemTime> waitlist;


// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_RTC0->EVENTS_COMPARE[0]) {
		do {
			auto time = Timer::next;
			next.value += 0xfffff;

			// resume all coroutines that where timeout occurred
			Timer::waitlist.resumeAll([time](SystemTime timeout) {
				if (timeout == time)
					return true;

				// check if this time is the next to elapse
				if (timeout < Timer::next)
					Timer::next = timeout;
				return false;
			});
			NRF_RTC0->CC[0] = Timer::next.value << 4;

			// repeat until next timeout is in the future
		} while (now() >= Timer::next);

		// clear pending interrupt flags at peripheral and NVIC
		NRF_RTC0->EVENTS_COMPARE[0] = 0;
		clearInterrupt(RTC0_IRQn);
	}

	// call next handler in chain
	Timer::nextHandler();
}

void init() {
	// check if already initialized
	if (Timer::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	Timer::nextHandler = Loop::addHandler(handle);

	// initialize RTC0
	next.value = 0xfffff;
	NRF_RTC0->CC[0] = next.value << 4;
	NRF_RTC0->EVTENSET = N(RTC_EVTENSET_OVRFLW, Set);
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE0, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	NRF_RTC0->TASKS_START = TRIGGER;
}

SystemTime now() {
	uint32_t counter = (NRF_RTC0->COUNTER + 8) >> 4;
	if (NRF_RTC0->EVENTS_OVRFLW) {
		NRF_RTC0->EVENTS_OVRFLW = 0;
		counter = (NRF_RTC0->COUNTER + 0x1000008) >> 4;
	}
	Timer::systemTime.value = (Timer::systemTime.value & 0xfff00000) + counter;
	return Timer::systemTime;
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if this time is the next to elapse
	if (time < Timer::next) {
		Timer::next = time;
		NRF_RTC0->CC[0] = time.value << 4;
	}

	// check if timeout already elapsed
	if (now() >= time) {
		NRF_RTC0->EVENTS_COMPARE[0] = GENERATED;
	}

	return {Timer::waitlist, time};
}

} // namespace Timer
