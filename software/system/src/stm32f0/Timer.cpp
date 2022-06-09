#include "../Timer.hpp"
#include "Loop.hpp"
#include <boardConfig.hpp>
#include <Debug.hpp>
#include <assert.hpp>


/*
	refernece manual: https://www.st.com/resource/en/reference_manual/dm00031936-stm32f0x1stm32f0x2stm32f0x8-advanced-armbased-32bit-mcus-stmicroelectronics.pdf

	Dependencies:

	Config:

	Resources:
		TIM2
			CCR1
*/
namespace Timer {

// next timeout of a timer in the list
SystemTime next;

// waiting coroutines
Waitlist<SystemTime> waitlist;


// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle() {
	if (TIM2->SR & TIM_SR_CC1IF) {
		do {
			// clear pending interrupt flags at peripheral and NVIC
			TIM2->SR = ~TIM_SR_CC1IF;
			clearInterrupt(TIM2_IRQn);

			auto time = Timer::next;
			Timer::next += SystemDuration::max();

			// resume all coroutines that where timeout occurred
			Timer::waitlist.resumeAll([time](SystemTime timeout) {
				if (timeout == time)
					return true;

				// check if this time is the next to elapse
				if (timeout < Timer::next)
					Timer::next = timeout;
				return false;
			});
			TIM2->CCR1 = Timer::next.value;

			// repeat until next timeout is in the future
		} while (SystemTime(TIM2->CNT) >= Timer::next);
	}

	// call next handler in chain
	Timer::nextHandler();
}

void init() {
	// check if already initialized
	if (Timer::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	Timer::nextHandler = Loop::addHandler(handle);

	// initialize TIM2
	RCC->APB1ENR = RCC->APB1ENR | RCC_APB1ENR_TIM2EN;
	Timer::next.value = SystemDuration::max().value;
	TIM2->CCR1 = Timer::next.value;
	TIM2->PSC = (CLOCK + 1000 / 2) / 1000 - 1;//(CLOCK + 1024 / 2) / 1024 - 1; // prescaler
	TIM2->EGR = TIM_EGR_UG; // update generation so that prescaler takes effect
	TIM2->DIER = TIM_DIER_CC1IE; // interrupt enable for CC1
	TIM2->CR1 = TIM_CR1_CEN; // enable, count up
}

SystemTime now() {
	return {TIM2->CNT};
}

Awaitable<SystemTime> sleep(SystemTime time) {
	// check if this time is the next to elapse
	if (time < Timer::next) {
		Timer::next = time;
		TIM2->CCR1 = time.value;
	}

	// check if timeout already elapsed
	if (SystemTime(TIM2->CNT) >= time)
		TIM2->EGR = TIM_EGR_CC1G; // trigger compare event

	return {Timer::waitlist, time};
}

} // namespace Timer
