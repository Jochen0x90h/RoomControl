#include "loop.hpp"
#include "global.hpp"
#include <assert.hpp>


namespace loop {

Handler nextHandler = waitForEvent;

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
}

Handler addHandler(Handler handler) {
	Handler h = nextHandler;
	loop::nextHandler = handler;
	return h;
}

void run() {
	while (true) {
		loop::nextHandler();
		/*
		// wait for next event
		waitForEvent();
	
		// call handlers
		for (int i = 0; i < handlerCount; ++i) {
			loop::handlers[i]();
		}*/
	}
}

} // namespace timer
