#include "../out.hpp"
#include "loop.hpp"
#include <util.hpp>
#include <sysConfig.hpp>
#include <stdio.h>


/*
	Config:
	OUT_COLORS: Array of colors for the emulated digital outputs
*/
namespace out {

bool states[array::count(OUT_COLORS)];


// event loop handler chain
loop::Handler nextHandler;
void handle(Gui &gui) {
	// call next handler in chain
	out::nextHandler(gui);

	// draw debug led's on gui
	gui.newLine();
	for (int i = 0; i < array::count(OUT_COLORS); ++i) {
		gui.led(out::states[i] ? OUT_COLORS[i] : 0);
	}
}

void init() {
	// add to event loop handler chain
	out::nextHandler = loop::addHandler(handle);
}

void set(int index, bool value) {
	assert(index >= 0 && index < array::count(OUT_COLORS));
	states[index] = value;
}

void toggle(int index) {
	assert(index >= 0 && index < array::count(OUT_COLORS));
	states[index] ^= true;
}

} // namespace out
