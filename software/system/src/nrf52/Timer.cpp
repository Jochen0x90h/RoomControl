#include "../Timer.hpp"
#include "Loop.hpp"
#include "nrf52.hpp"
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

// timer interval is 1024 seconds (2^24 / 16384Hz), given in milliseconds
constexpr int INTERVAL = 1024000;

uint32_t baseTime = 0;

// next timeout of a timer in the list
SystemTime next;

// waiting coroutines
Waitlist<SystemTime> waitlist;


// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_RTC0->EVENTS_COMPARE[0]) {
		do {
			// clear pending interrupt flags at peripheral and NVIC
			NRF_RTC0->EVENTS_COMPARE[0] = 0;
			clearInterrupt(RTC0_IRQn);

			auto time = Timer::next;
			Timer::next.value += INTERVAL - 1;

			// resume all coroutines that where timeout occurred
			Timer::waitlist.resumeAll([time](SystemTime timeout) {
				if (timeout == time)
					return true;

				// check if this time is the next to elapse
				if (timeout < Timer::next)
					Timer::next = timeout;
				return false;
			});
			NRF_RTC0->CC[0] = ((Timer::next.value - Timer::baseTime) << (7 + 4)) / 125;//Timer::next.value << 4;

			// repeat until next timeout is in the future
		} while (now() >= Timer::next);
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
	Timer::next.value = INTERVAL - 1;
	NRF_RTC0->CC[0] = next.value << 4;
	NRF_RTC0->EVTENSET = N(RTC_EVTENSET_OVRFLW, Set);
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE0, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	NRF_RTC0->TASKS_START = TRIGGER;
}

SystemTime now() {
	// time resolution 1/1000 s
	uint32_t counter = NRF_RTC0->COUNTER;
	if (NRF_RTC0->EVENTS_OVRFLW) {
		NRF_RTC0->EVENTS_OVRFLW = 0;

		// reload counter in case overflow happened after reading the counter
		counter = NRF_RTC0->COUNTER;

		// advance base time by one interval (1024 seconds)
		Timer::baseTime += INTERVAL;
	}
	return {Timer::baseTime + ((counter * 125 + 1024) >> (7 + 4))};
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if this time is the next to elapse
	if (time < Timer::next) {
		Timer::next = time;
		NRF_RTC0->CC[0] = ((time.value - Timer::baseTime) << (7 + 4)) / 125;
	}

	// check if timeout already elapsed
	if (now() >= time)
		NRF_RTC0->EVENTS_COMPARE[0] = GENERATED;

	return {Timer::waitlist, time};
}

} // namespace Timer
