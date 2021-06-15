#include "loop.hpp"
#include "global.hpp"
#include <assert.hpp>


namespace loop {

Handler nextHandler = waitForEvent;
Handler addHandler(Handler handler) {
	Handler h = nextHandler;
	loop::nextHandler = handler;
	return h;
}

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

void run() {
	while (true) {
		// call handler chain of drivers
		loop::nextHandler();
	}
}

} // namespace timer
