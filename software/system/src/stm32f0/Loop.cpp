#include "Loop.hpp"
#include "defs.hpp"


// called from system/startup_stm32f042x6.s to setup clock and flash before static constructors
void SystemInit(void) {
	// leave clock in default configuration (8MHz)
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
	// disabled interrupts trigger an event and wake up the processor from WFE
	// see chapter 5.3.3 in reference manual
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

} // namespace Loop
