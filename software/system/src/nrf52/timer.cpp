#include "../timer.hpp"
#include "loop.hpp"
#include "global.hpp"
#include <assert.hpp>


namespace timer {

SystemTime systemTime;

struct Timer {
	// state: 0 = not allocated, 1 = stopped, 2 = running
	uint8_t state = 0;
	
	// timeout time
	SystemTime time;
	
	// timeout callback
	std::function<void ()> onTimeout;
};

Timer timers[TIMER_COUNT];
SystemTime next;

// event loop handler chain
loop::Handler nextHandler;
void handle() {
	if (NRF_RTC0->EVENTS_COMPARE[0]) {
		do {
			auto activate = next;
			next.value += 0x80000;
			for (Timer &timer : timers) {
				// check if timer is running
				if (timer.state == 2) {
					if (timer.time == activate) {
						// activate this timer
						timer.state = 1;
						if (timer.onTimeout)
							timer.onTimeout();
					} else {
						// check if this timer is the next to elapse
						if (timer.time < next)
							next = timer.time;
					}
				}
			}
			NRF_RTC0->CC[0] = next.value << 4;
		} while (getTime() >= next);

		// clear pending interrupt flags at peripheral and NVIC
		NRF_RTC0->EVENTS_COMPARE[0] = 0;
		clearInterrupt(RTC0_IRQn);
	}
	
	// call next handler in chain
	timer::nextHandler();
}

void init() {
	// use channel 0 of RTC0
	next.value = 0x80000;
	NRF_RTC0->CC[0] = next.value << 4;
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE0, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	NRF_RTC0->TASKS_START = Trigger;
	
	// add to event loop handler chain
	timer::nextHandler = loop::addHandler(handle);
}

SystemTime getTime() {
	uint32_t counter = ((NRF_RTC0->COUNTER + 8) >> 4) & 0xfffff;
	bool overflow = counter < (systemTime.value & 0xfffff);
	systemTime.value = ((systemTime.value & 0xfff00000) | counter) + (overflow ? 0x0100000 : 0);
	return systemTime;
}

uint8_t allocate() {
	for (uint8_t i = 0; i < TIMER_COUNT; ++i) {
		Timer &timer = timer::timers[i];
		if (timer.state == 0) {
			timer.state = 1;
			return i + 1;
		}
	}
	
	// error: timer array is full
	assert(false);
	return 0;
}

void setHandler(uint8_t id, std::function<void ()> const &onTimeout) {
	int index = id - 1;
	assert(index >= 0 && index < TIMER_COUNT);
	Timer &timer = timer::timers[index];
	assert(timer.state > 0);
	timer.onTimeout = onTimeout;
}

void start(uint8_t id, SystemTime time) {
	int index = id - 1;
	assert(index >= 0 && index < TIMER_COUNT);
	Timer &timer = timer::timers[index];
	assert(timer.state > 0);

	// set state to running and timeout time
	timer.state = 2;
	timer.time = time;
		
	// check if this timer is the next to elapse
	if (time < next) {
		next = time;
		NRF_RTC0->CC[0] = next.value << 4;
	}

	// check if timeout already elapsed
	if (time > getTime()) {
		NRF_RTC0->EVENTS_COMPARE[0] = Generated;
	}
}

void stop(uint8_t id) {
	int index = id - 1;
	assert(index >= 0 && index < TIMER_COUNT);
	Timer &timer = timer::timers[index];
	assert(timer.state > 0);

	// set state to stopped
	timer.state = 1;
}

} // namespace timer
