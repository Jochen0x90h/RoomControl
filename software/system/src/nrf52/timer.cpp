#include "../timer.hpp"
#include "loop.hpp"
#include "global.hpp"
#include <assert.hpp>


namespace timer {

SystemTime systemTime;

enum State : uint8_t {
	NOT_ALLOCATED = 0,
	STOPPED = 1,
	RUNNING = 2
};

struct Timer {
	// state
	State state = NOT_ALLOCATED;
	
	// timeout time
	SystemTime time;
	
	// timeout callback
	std::function<void ()> onTimeout;
};

// list of timers
Timer timers[TIMER_COUNT];

// next timeout of a timer in the list
SystemTime next;


// event loop handler chain
loop::Handler nextHandler;
void handle() {
	if (NRF_RTC0->EVENTS_COMPARE[0]) {
		do {
			auto timeout = timer::next;
			next.value += 0x80000;
			for (Timer &t : timers) {
				// check if timer is running
				if (t.state == RUNNING) {
					// check if timeout time reached
					if (t.time == timeout) {
						// stop timer
						t.state = STOPPED;
						
						// call handler
						if (t.onTimeout)
							t.onTimeout();
					} else {
						// check if this timer is the next to elapse
						if (t.time < timer::next)
							timer::next = t.time;
					}
				}
			}
			NRF_RTC0->CC[0] = timer::next.value << 4;
		} while (getTime() >= timer::next);

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
		Timer &t = timer::timers[i];
		if (t.state == NOT_ALLOCATED) {
			t.state = STOPPED;
			return i + 1;
		}
	}
	
	// error: timer array is full
	assert(false);
	return 0;
}

void setHandler(uint8_t id, std::function<void ()> const &onTimeout) {
	uint8_t index = id - 1;
	assert(index < TIMER_COUNT);
	Timer &t = timer::timers[index];
	assert(t.state != NOT_ALLOCATED);
	t.onTimeout = onTimeout;
}

void start(uint8_t id, SystemTime time) {
	uint8_t index = id - 1;
	assert(index < TIMER_COUNT);
	Timer &t = timer::timers[index];
	assert(t.state != NOT_ALLOCATED);

	// set state to running
	t.state = RUNNING;
	
	// set timeout time
	t.time = time;
		
	// check if this timer is the next to elapse
	if (time < timer::next) {
		timer::next = time;
		NRF_RTC0->CC[0] = timer::next.value << 4;
	}

	// check if timeout already elapsed
	if (time > getTime()) {
		NRF_RTC0->EVENTS_COMPARE[0] = Generated;
	}
}

void stop(uint8_t id) {
	uint8_t index = id - 1;
	assert(index < TIMER_COUNT);
	Timer &t = timer::timers[index];
	assert(t.state != NOT_ALLOCATED);

	// set state to stopped
	t.state = STOPPED;
}

} // namespace timer
