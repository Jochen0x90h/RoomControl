#include "../Input.hpp"
#include "Loop.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	Config:
	OUT_COLORS: Array of colors for the emulated digital outputs
*/
namespace Input {

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
	Input::waitlist.resumeAll([risingFlag, fallingFlag, index, value] (Parameters const &p) {
		if ((p.risingFlags & risingFlag) != 0 || (p.fallingFlags & fallingFlag) != 0) {
			p.index = index;
			p.value = value;
			return true;
		}
		return false;
	});
}


// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle(Gui &gui) {
	// call next handler in chain
	Input::nextHandler(gui);

	#ifdef INPUT_DOUBLE_ROCKER
	{
		// emulate a switch on the inputs
		int value = gui.doubleRocker(0x47b8628e);
		if (value != -1) {
			set(0, (value & 1) != 0);
			set(1, (value & 2) != 0);
			set(2, (value & 4) != 0);
			set(3, (value & 8) != 0);
		}
	}
	#endif
}

void init() {
	// check if already initialized
	if (Input::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	Input::nextHandler = Loop::addHandler(handle);

	// set initial state using config
	for (int i = 0; i < INPUT_COUNT; ++i) {
		State &state = Input::states[i];
		state.value = false;
	}
}

bool read(int index) {
	assert(Input::nextHandler != nullptr && uint32_t(index) < INPUT_COUNT); // init() not called or index out of range
	return Input::states[index].value;
}

Awaitable<Parameters> trigger(uint32_t risingFlags, uint32_t fallingFlags, int &index, bool &value) {
	assert(Input::nextHandler != nullptr); // init() not called
	return {Input::waitlist, risingFlags, fallingFlags, index, value};
}

} // namespace Input
