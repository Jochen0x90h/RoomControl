#include "../Input.hpp"
#include "../Timer.hpp"
#include "Loop.hpp"
#include "gpio.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	Dependencies:
		Timer

	Config:
		INPUTS: Array of InputConfig
		INPUT_COUNT: Number of inputs
		TRIGGER_COUNT: Number of inputs that support trigger()
			Note: Pin index must be unique for each trigger, i.e. PA(0) and PB(0) can't be triggers at the same time 
		
	Resources:
		GPIO
		TIM2
			CCR2
*/
namespace Input {

// next timeout of an input
SystemTime next;

// states for debounce filter
struct State {	
	SystemTime timeout;
	bool value;
};
State states[TRIGGER_COUNT];

// waiting coroutines
Waitlist<Parameters> waitlist;

// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle() {	
	int PR = EXTI->PR;
	if ((PR & 0xffff) != 0) {
		// debounce timeout
		auto timeout = Timer::now() + 50ms;
		for (int index = 0; index < TRIGGER_COUNT; ++index) {
			auto &input = INPUTS[index];
			int pos = input.pin & 15;
			if (PR & (1 << pos)) {
				auto &state = states[index];			

				// set debounce timeout
				state.timeout = timeout;
				
				// check if this is the next timeout
				if (timeout < Input::next) {
					Input::next = timeout;
					TIM2->CCR2 = timeout.value;
				}
			}		
		}

		// clear pending interrupt flags at peripheral and NVIC
		EXTI->PR = 0xffff;
		clearInterrupt(EXTI0_1_IRQn);
		clearInterrupt(EXTI2_3_IRQn);
		clearInterrupt(EXTI4_15_IRQn);
	}
	if (TIM2->SR & TIM_SR_CC2IF) {
		do {
			// clear pending interrupt flags at peripheral and NVIC
			TIM2->SR = ~TIM_SR_CC2IF;
			clearInterrupt(TIM2_IRQn);

			// get current time
			auto time = Input::next;
			
			// add RTC interval
			Input::next.value += 0x7fffffff;
			
			for (int index = 0; index < TRIGGER_COUNT; ++index) {
				auto &input = INPUTS[index];
				auto &state = states[index];
				
				// check if debounce timeout for this input elapsed
				if (state.timeout == time) {
					state.timeout.value += 0x7fffffff;

					// read input value
					bool value = gpio::readInput(input.pin) != input.invert;
					bool old = state.value;
					state.value = value;

					uint32_t risingFlag = int(value && !old) << index;
					uint32_t fallingFlag = int(!value && old) << index;					

					// resume coroutines that wait for an event on this input
					Input::waitlist.resumeAll([risingFlag, fallingFlag, index, value] (Parameters const &p) {
						if ((p.risingFlags & risingFlag) != 0 || (p.fallingFlags & fallingFlag) != 0) {
							p.index = index;
							p.value = value;
							return true;
						}
						return false;
					});
				} else if (state.timeout < Input::next) {
					// this input is next
					Input::next = state.timeout;
				}
			}
			TIM2->CCR2 = Input::next.value;
		
			// repeat until next timeout is in the future
		} while (Timer::now() >= Input::next);
	}
	
	// call next handler in chain
	Input::nextHandler();
}

void init() {
	// configure inputs
	for (auto &input : INPUTS) {
		addInputConfig(input);
	}

	// configure triggers
	if (TRIGGER_COUNT > 0) {
		Timer::init();
	
		// check if already initialized
		if (Input::nextHandler != nullptr)
			return;
	
		// add to event loop handler chain
		Input::nextHandler = Loop::addHandler(handle);
	
		for (int index = 0; index < TRIGGER_COUNT; ++index) {
			auto &input = INPUTS[index];
			auto &state = states[index];
			int port = input.pin >> 4;
			int pin = input.pin & 15;
	
			state.timeout.value = 0x7fffffff;

			// configure event
			// see chapter 11.2 in reference manual and A.6.2 for code example
			auto &EXTICR = SYSCFG->EXTICR[pin >> 2];
			EXTICR = EXTICR | (port << (pin & 3));
			EXTI->FTSR = EXTI->FTSR | 1 << pin; // detect falling edge
			EXTI->RTSR = EXTI->RTSR | 1 << pin; // detect rising edge
			EXTI->PR = 1 << pin; // clear pending interrupt			
			EXTI->IMR = EXTI->IMR | 1 << pin; // enable interrupt
				
			// read input value
			state.value = gpio::readInput(input.pin) != input.invert;
		}
	
		// use channel 2 of TIM2 (on top of Timer::init())
		Input::next.value = 0x7fffffff;
		TIM2->CCR2 = Input::next.value;
		TIM2->DIER = TIM2->DIER | TIM_DIER_CC2IE; // interrupt enable
	}
}

bool read(int index) {
	if (uint32_t(index) < INPUT_COUNT) {
		auto &input = INPUTS[index];
		return gpio::readInput(input.pin) != input.invert;
	}
	return false;
}

Awaitable<Parameters> trigger(uint32_t risingFlags, uint32_t fallingFlags, int &index, bool &value) {
	return {Input::waitlist, risingFlags, fallingFlags, index, value};
}

} // namespace Input
