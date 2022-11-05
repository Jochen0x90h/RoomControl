#include "Loop.hpp"
#include "nrf52.hpp"


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

namespace Loop {

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
	Loop::nextHandler = handler;
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
}

void run() {
	while (true) {
		// call all handlers
		auto it = Loop::handlers.begin();
		while (it != Loop::handlers.end()) {
			// increment iterator beforehand because a handler can remove() itself
			auto current = it;
			++it;
			current->handle();
		}

		// wait for something to happen
		// waitForEvent();

		// call handler chain of drivers
		Loop::nextHandler();
	}
}

void busyWait(int us) {
	auto cycles = us * 8;
	for (int i = 0; i < cycles; ++i) {
		__NOP();
	}
}

} // namespace Loop
