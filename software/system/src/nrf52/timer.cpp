#include "../timer.hpp"
#include "loop.hpp"
#include "global.hpp"
#include <assert.hpp>


namespace timer {

SystemTime systemTime;

// next timeout of a timer in the list
SystemTime next;

// waiting coroutines
Waitlist<SystemTime> waitlist;


// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle() {
	if (NRF_RTC0->EVENTS_COMPARE[0]) {
		do {
			auto timeout = timer::next;
			next.value += 0x80000;
			
			// resume all coroutines that where timeout occurred
			timer::waitlist.resumeAll([timeout](SystemTime time) {
				if (time == timeout)
					return true;
				
				// check if this time is the next to elapse
				if (time < timer::next)
					timer::next = time;
				return false;
			});
			NRF_RTC0->CC[0] = timer::next.value << 4;
		
			// repeat until next timeout is in the future
		} while (now() >= timer::next);

		// clear pending interrupt flags at peripheral and NVIC
		NRF_RTC0->EVENTS_COMPARE[0] = 0;
		clearInterrupt(RTC0_IRQn);
	}
	
	// call next handler in chain
	timer::nextHandler();
}

void init() {
	// check if already initialized
	if (timer::nextHandler != nullptr)
		return;

	// use channel 0 of RTC0
	next.value = 0x80000;
	NRF_RTC0->CC[0] = next.value << 4;
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE0, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	NRF_RTC0->TASKS_START = Trigger;
	
	// add to event loop handler chain
	timer::nextHandler = loop::addHandler(handle);
}

SystemTime now() {
	uint32_t counter = ((NRF_RTC0->COUNTER + 8) >> 4) & 0xfffff;
	bool overflow = counter < (timer::systemTime.value & 0xfffff);
	timer::systemTime.value = ((systemTime.value & 0xfff00000) | counter) + (overflow ? 0x0100000 : 0);
	return timer::systemTime;
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if this time is the next to elapse
	if (time < timer::next) {
		timer::next = time;
		NRF_RTC0->CC[0] = timer::next.value << 4;
	}

	// check if timeout already elapsed
	if (time > now()) {
		NRF_RTC0->EVENTS_COMPARE[0] = Generated;
	}
	
	return {timer::waitlist, time};
}

} // namespace timer
