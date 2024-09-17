#include "Loop.hpp"
#include "defs.hpp"
#include "../SystemTime.hpp"
#include <boardConfig.hpp>


/*
	refernece manual: https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf

	Dependencies:

	Config:

	Resources:
		TIM2
			CCR1
*/

// called from system/startup_stm32f042x6.s to setup clock and flash before static constructors
void SystemInit(void) {
	// leave clock in default configuration (8MHz)
}

namespace loop {

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
	// disabled interrupts trigger an event and wake up the processor from WFE
	// see chapter 5.3.3 in reference manual
	SCB->SCR = SCB->SCR | SCB_SCR_SEVONPEND_Msk;

	// initialize TIM2
	RCC->APB1ENR = RCC->APB1ENR | RCC_APB1ENR_TIM2EN;
	loop::next.value = SystemDuration::max().value;
	TIM2->CCR1 = loop::next.value;
	TIM2->PSC = (CLOCK + 1000 / 2) / 1000 - 1; // prescaler for 1ms timer resolution
	TIM2->EGR = TIM_EGR_UG; // update generation so that prescaler takes effect
	TIM2->DIER = TIM_DIER_CC1IE; // interrupt enable for CC1
	TIM2->CR1 = TIM_CR1_CEN; // enable, count up
}

void run() {
	while (true) {
		// check if a sleep time has elapsed
		if (TIM2->SR & TIM_SR_CC1IF) {
			do {
				// clear pending interrupt flags at peripheral and NVIC
				TIM2->SR = ~TIM_SR_CC1IF;
				clearInterrupt(TIM2_IRQn);

				auto time = loop::next;
				loop::next += SystemDuration::max();

				// resume all coroutines that where timeout occurred
				loop::waitlist.resumeAll([time](SystemTime timeout) {
					if (timeout == time)
						return true;

					// check if this time is the next to elapse
					if (timeout < loop::next)
						loop::next = timeout;
					return false;
				});
				TIM2->CCR1 = loop::next.value;

				// repeat until next timeout is in the future
			} while (SystemTime(TIM2->CNT) >= loop::next);
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
	return {TIM2->CNT};
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if this time is the next to elapse
	if (time < loop::next) {
		loop::next = time;
		TIM2->CCR1 = time.value;
	}

	// check if timeout already elapsed
	if (SystemTime(TIM2->CNT) >= time)
		TIM2->EGR = TIM_EGR_CC1G; // trigger compare event

	return {loop::waitlist, time};
}

} // namespace loop
