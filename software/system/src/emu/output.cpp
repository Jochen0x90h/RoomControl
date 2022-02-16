#include "../output.hpp"
#include "loop.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	Config:
	OUT_COLORS: Array of colors for the emulated digital outputs
*/
namespace output {

struct State {
	uint32_t color;
	bool enabled;
	bool value;
};

State states[OUTPUT_COUNT];


// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle(Gui &gui) {
	// call next handler in chain
	output::nextHandler(gui);

	// draw outputs as virtual led's
	gui.newLine();
	for (State &state : output::states) {
		gui.led(state.value ? state.color : 0x00101010);
	}
}

void init() {
	// check if already initialized
	if (output::nextHandler != nullptr)
		return;
	
	// add to event loop handler chain
	output::nextHandler = loop::addHandler(handle);
	
	// set initial state using config
	for (int i = 0; i < OUTPUT_COUNT; ++i) {
		State &state = output::states[i];
		state.color = OUTPUTS[i].color;
		state.enabled = OUTPUTS[i].enabled;
		state.value = OUTPUTS[i].initialValue;
	}
}

void enable(int index, bool enabled) {
	assert(output::nextHandler != nullptr && uint32_t(index) < OUTPUT_COUNT); // init() not called or index out of range
	output::states[index].enabled = enabled;
}

void set(int index, bool value) {
	assert(output::nextHandler != nullptr && uint32_t(index) < OUTPUT_COUNT); // init() not called or index out of range
	output::states[index].value = value;
}

void toggle(int index) {
	assert(output::nextHandler != nullptr && uint32_t(index) < OUTPUT_COUNT); // init() not called or index out of range
	output::states[index].value ^= true;
}

} // namespace output
