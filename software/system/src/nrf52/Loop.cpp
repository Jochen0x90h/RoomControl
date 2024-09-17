#include "Loop.hpp"
#include "nrf52.hpp"


/*
	https://infocenter.nordicsemi.com/topic/struct_nrf52/struct/nrf52840.html

	Dependencies:

	Config:

	Resources:
		NRF_RTC0
			CC[0]
*/

// called by system/gcc_startup_nrf52840.S
extern "C" {
void SystemInit() {
	// enable FPU (see nRF5_SDK_*/modules/nrfx/mdk/system_nrf52.c)
	#if (__FPU_USED == 1)
		SCB->CPACR = SCB->CPACR | (3UL << 20) | (3UL << 22);
		__DSB();
		__ISB();
	#else
		#warning "FPU is not used"
	#endif
}
}

namespace loop {

// timer interval is 1024 seconds (2^24 / 16384Hz), given in milliseconds
constexpr int INTERVAL = 1024000;

uint32_t baseTime = 0;

// next timeout of a timer in the list
SystemTime next;

// waiting coroutines
Waitlist<SystemTime> waitlist;


// wait for event
// see http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dai0321a/BIHICBGB.html
void waitForEvent() {
	// data synchronization barrier
	__DSB();
	
	// wait for event (interrupts trigger an event due to SEVONPEND)
	__WFE();
}


Handler nextHandler = waitForEvent;
Handler addHandler(Handler handler) {
	Handler h = nextHandler;
	loop::nextHandler = handler;
	return h;
}

HandlerList handlers;

void init() {
	// configure system clock
	NRF_CLOCK->HFXODEBOUNCE = N(CLOCK_HFXODEBOUNCE_HFXODEBOUNCE, Db1024us);
	NRF_CLOCK->LFCLKSRC = N(CLOCK_LFCLKSRC_SRC, Synth);

	// start HF clock and wait
	NRF_CLOCK->TASKS_HFCLKSTART = TRIGGER;
	while (!NRF_CLOCK->EVENTS_HFCLKSTARTED);
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

	// start LF clock and wait
	NRF_CLOCK->TASKS_LFCLKSTART = TRIGGER;
	while (!NRF_CLOCK->EVENTS_LFCLKSTARTED);
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
		
	// disabled interrupts trigger an event and wake up the processor from WFE
	SCB->SCR = SCB->SCR | SCB_SCR_SEVONPEND_Msk;

	// initialize RTC0
	loop::next.value = INTERVAL - 1;
	NRF_RTC0->CC[0] = next.value << 4;
	NRF_RTC0->EVTENSET = N(RTC_EVTENSET_OVRFLW, Set);
	NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE0, Set);
	NRF_RTC0->PRESCALER = 1; // 16384Hz
	NRF_RTC0->TASKS_START = TRIGGER;
}

void run() {
	while (true) {
		// check if a sleep time has elapsed
		if (NRF_RTC0->EVENTS_COMPARE[0]) {
			do {
				// clear pending interrupt flags at peripheral and NVIC
				NRF_RTC0->EVENTS_COMPARE[0] = 0;
				clearInterrupt(RTC0_IRQn);

				auto time = loop::next;
				loop::next.value += INTERVAL - 1;

				// resume all coroutines that where timeout occurred
				loop::waitlist.resumeAll([time](SystemTime timeout) {
					if (timeout == time)
						return true;

					// check if this time is the next to elapse
					if (timeout < loop::next)
						loop::next = timeout;
					return false;
				});
				NRF_RTC0->CC[0] = ((loop::next.value - loop::baseTime) << (7 + 4)) / 125;//Timer::next.value << 4;

				// repeat until next timeout is in the future
			} while (now() >= loop::next);
		}

		// call all handlers
		auto it = loop::handlers.begin();
		while (it != loop::handlers.end()) {
			// increment iterator beforehand because a handler can remove() itself
			auto current = it;
			++it;
			current->handle();
		}

		// wait for something to happen
		// waitForEvent();

		// call handler chain of drivers
		loop::nextHandler();
	}
}

SystemTime now() {
	// time resolution 1/1000 s
	uint32_t counter = NRF_RTC0->COUNTER;
	if (NRF_RTC0->EVENTS_OVRFLW) {
		NRF_RTC0->EVENTS_OVRFLW = 0;

		// reload counter in case overflow happened after reading the counter
		counter = NRF_RTC0->COUNTER;

		// advance base time by one interval (1024 seconds)
		loop::baseTime += INTERVAL;
	}
	return {loop::baseTime + ((counter * 125 + 1024) >> (7 + 4))};
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if this time is the next to elapse
	if (time < loop::next) {
		loop::next = time;
		NRF_RTC0->CC[0] = ((time.value - loop::baseTime) << (7 + 4)) / 125;
	}

	// check if timeout already elapsed
	if (now() >= time)
		NRF_RTC0->EVENTS_COMPARE[0] = GENERATED;

	return {loop::waitlist, time};
}

void sleepBlocking(int us) {
	auto cycles = us * 8;
	for (int i = 0; i < cycles; ++i) {
		__NOP();
	}
}

} // namespace loop
