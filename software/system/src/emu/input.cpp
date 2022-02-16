#include "../input.hpp"
#include "loop.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	Config:
	OUT_COLORS: Array of colors for the emulated digital outputs
*/
namespace input {

struct State {
	bool value;
	//TriggerMode triggerMode;
};
Waitlist<Parameters> waitlist;

State states[INPUT_COUNT];


// internal interface
void set(int index, bool value) {
	assert(index >= 0 && index < INPUT_COUNT);
	auto &state = states[index];

	bool old = state.value;
	state.value = value;
	uint32_t risingFlag = int(value && !old) << index;
	uint32_t fallingFlag = int(!value && old) << index;

	// resume coroutines that wait for an event on this input
	input::waitlist.resumeAll([risingFlag, fallingFlag, index, value] (Parameters const &p) {
		if ((p.risingFlags & risingFlag) != 0 || (p.fallingFlags & fallingFlag) != 0) {
			p.index = index;
			p.value = value;
			return true;
		}
		return false;
	});
}


// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle(Gui &gui) {
	// call next handler in chain
	input::nextHandler(gui);

}

void init() {
	// check if already initialized
	if (input::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	input::nextHandler = loop::addHandler(handle);

	// set initial state using config
	for (int i = 0; i < INPUT_COUNT; ++i) {
		State &state = input::states[i];
		state.value = false;
	}
}

bool read(int index) {
	assert(input::nextHandler != nullptr && uint32_t(index) < INPUT_COUNT); // init() not called or index out of range
	return input::states[index].value;
}

Awaitable<Parameters> trigger(uint32_t risingFlags, uint32_t fallingFlags, int &index, bool &value) {
	assert(input::nextHandler != nullptr); // init() not called
	return {input::waitlist, risingFlags, fallingFlags, index, value};
}

} // namespace input
