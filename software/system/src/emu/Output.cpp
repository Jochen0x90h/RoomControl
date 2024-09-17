#include "../Output.hpp"
#include "Loop.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	Config:
	OUT_COLORS: Array of colors for the emulated digital outputs
*/
namespace Output {

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
	Output::nextHandler(gui);

	// draw outputs as virtual led's
	gui.newLine();
	for (State &state : Output::states) {
		gui.led(state.value ? state.color : 0x00101010);
	}
}

void init() {
	// check if already initialized
	if (Output::nextHandler != nullptr)
		return;
	
	// add to event loop handler chain
	Output::nextHandler = loop::addHandler(handle);
	
	// set initial state using config
	for (int i = 0; i < OUTPUT_COUNT; ++i) {
		State &state = Output::states[i];
		state.color = OUTPUTS[i].color;
		state.enabled = OUTPUTS[i].enabled;
		state.value = OUTPUTS[i].initialValue;
	}
}

void enable(int index, bool enabled) {
	assert(Output::nextHandler != nullptr && uint32_t(index) < OUTPUT_COUNT); // init() not called or index out of range
	Output::states[index].enabled = enabled;
}

void set(int index, bool value) {
	assert(Output::nextHandler != nullptr && uint32_t(index) < OUTPUT_COUNT); // init() not called or index out of range
	Output::states[index].value = value;
}

void toggle(int index) {
	assert(Output::nextHandler != nullptr && uint32_t(index) < OUTPUT_COUNT); // init() not called or index out of range
	Output::states[index].value ^= true;
}

} // namespace Output
