#include "../Input.hpp"
#include "Loop.hpp"
#include "gpio.hpp"
#include <util.hpp>
#include <boardConfig.hpp>
#include <Debug.hpp>


/*
	Dependencies:
		Timer

	Config:
		INPUTS: Array of InputConfig
		INPUT_COUNT: Number of inputs
		TRIGGER_COUNT: Number of inputs that support trigger()
		
	Resources:
		GPIO
		NRF_GPIOTE
			IN[0-7]
		NRF_RTC0
			CC[1]
*/
namespace Input {

static_assert(TRIGGER_COUNT <= INPUT_COUNT);
static_assert(TRIGGER_COUNT <= 8);

// next timeout of an input
int next;

// states for debounce filter
struct State {	
	int timeout;
	bool value;
};
State states[TRIGGER_COUNT];

// waiting coroutines
Waitlist<Parameters> waitlist;

// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle() {	
	if (isInterruptPending(GPIOTE_IRQn)) {
		// debounce timeout after about 50ms
		int timeout = NRF_RTC0->COUNTER + 50 * 16;
		for (int index = 0; index < TRIGGER_COUNT; ++index) {
			if (NRF_GPIOTE->EVENTS_IN[index]) {
				// clear pending interrupt flags at peripheral
				NRF_GPIOTE->EVENTS_IN[index] = 0;

				auto &state = states[index];			

				// set debounce timeout
				state.timeout = timeout;
				
				// check if this is the next timeout
				if (((timeout - Input::next) << 8) < 0) {
					Input::next = timeout;
					NRF_RTC0->CC[1] = timeout;
				}
			}		
		}

		// clear pending interrupt at NVIC
		clearInterrupt(GPIOTE_IRQn);
	}
	if (NRF_RTC0->EVENTS_COMPARE[1]) {
		do {
			// clear pending interrupt flags at peripheral and NVIC
			NRF_RTC0->EVENTS_COMPARE[1] = 0;
			clearInterrupt(RTC0_IRQn);

			// get current time
			int time = Input::next;
			
			// add RTC interval (is 24 bit)
			Input::next += 0x7fffff;
			
			for (int index = 0; index < TRIGGER_COUNT; ++index) {
				auto &input = INPUTS[index];
				auto &state = states[index];
				
				// check if debounce timeout for this input elapsed
				if (state.timeout == time) {
					state.timeout += 0x7fffff;

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
			NRF_RTC0->CC[1] = Input::next;
		
			// repeat until next timeout is in the future
		} while (((int(NRF_RTC0->COUNTER) - Input::next) << 8) >= 0);
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
		//Timer::init();
	
		// check if already initialized
		if (Input::nextHandler != nullptr)
			return;
	
		// add to event loop handler chain
		Input::nextHandler = loop::addHandler(handle);
	
		for (int index = 0; index < TRIGGER_COUNT; ++index) {
			auto &input = INPUTS[index];
			auto &state = states[index];
	
			state.timeout = 0x7fffff;

			// configure event
			NRF_GPIOTE->CONFIG[index] = N(GPIOTE_CONFIG_MODE, Event)
				| N(GPIOTE_CONFIG_POLARITY, Toggle)
				| V(GPIOTE_CONFIG_PSEL, input.pin);
				
			// read input value
			state.value = gpio::readInput(input.pin) != input.invert;
	
			// clear pending event
			NRF_GPIOTE->EVENTS_IN[index] = 0;
		}
	
		// enable GPIOTE interrupts
		NRF_GPIOTE->INTENSET = ~(0xffffffff << TRIGGER_COUNT);

		// use channel 1 of RTC0 (on top of Timer::init())
		Input::next = 0x7fffff;
		NRF_RTC0->CC[1] = Input::next;
		NRF_RTC0->INTENSET = N(RTC_INTENSET_COMPARE1, Set);
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
