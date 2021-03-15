#include "../timer.hpp"
#include "global.hpp"


namespace timer {

SystemTime systemTime;

struct Timer {
	bool active;
	SystemTime time;
	std::function<void ()> onTimeout;
	
	Timer() : onTimeout([]() {}) {}
};

Timer timers[TIMER_COUNT];
SystemTime next;

void init() {
	// configure system clock
	NRF_CLOCK->HFXODEBOUNCE = N(CLOCK_HFXODEBOUNCE_HFXODEBOUNCE, Db1024us);
	NRF_CLOCK->LFCLKSRC = N(CLOCK_LFCLKSRC_SRC, Synth);

	// start HF clock and wait
	NRF_CLOCK->TASKS_HFCLKSTART = Trigger;
	while (!NRF_CLOCK->EVENTS_HFCLKSTARTED);
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

	// start LF clock and wait
	NRF_CLOCK->TASKS_LFCLKSTART = Trigger;
	while (!NRF_CLOCK->EVENTS_LFCLKSTARTED);
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
		
	// disabled interrupts trigger an event and wake up the processor from WFE
	SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

	// system time
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE0, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	next.value = 0x80000;
	NRF_RTC0->CC[0] = next.value << 4;
	NRF_RTC0->TASKS_START = Trigger;
}

void handle() {
	// wait for next event
	waitForEvent();
	
	if (NRF_RTC0->EVENTS_COMPARE[0]) {
		do {
			auto activate = next;
			next.value += 0x80000;
			for (Timer &timer : timers) {
				if (timer.active) {
					if (timer.time == activate) {
						// activate this timer
						timer.active = false;
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
}

SystemTime getTime() {
	uint32_t counter = ((NRF_RTC0->COUNTER + 8) >> 4) & 0xfffff;
	bool overflow = counter < (systemTime.value & 0xfffff);
	systemTime.value = ((systemTime.value & 0xfff00000) | counter) + (overflow ? 0x0100000 : 0);
	return systemTime;
}

void setHandler(int index, std::function<void ()> const &onTimeout) {
	if (uint32_t(index) < uint32_t(TIMER_COUNT)) {
		Timer &timer = timers[index];
		timer.onTimeout = onTimeout;
	}
}

void start(int index, SystemTime time) {
	if (uint32_t(index) < uint32_t(TIMER_COUNT)) {
		Timer &timer = timers[index];
		timer.active = true;
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
}

void stop(int index) {
	if (uint32_t(index) < uint32_t(TIMER_COUNT))
		timers[index].active = false;
}

} // namespace timer
