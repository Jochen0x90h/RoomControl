#include "../debug.hpp"
#include "loop.hpp"
#include <stdio.h>


namespace debug {

bool red = false;
bool green = false;
bool blue = false;
/*
bool changed = false;

void print() {
	printf("\r");
	printf(debug::red ? "📕\t" : " \t");
	printf(debug::green ? "📗\t" : " \t");
	printf(debug::blue ? "📘" : " ");
	//printf("\n");
	fflush(stdout);
	debug::changed = false;
}

void set(bool &value, bool newValue) {
	if (value != newValue && !debug::changed) {
		debug::changed = true;
		loop::context.post([]() {print();});
	}
	value = newValue;
}
*/

inline void set(bool &value, bool newValue) {value = newValue;}


// event loop handler chain
loop::Handler nextHandler;
void handle(Gui &gui) {
	// call next handler in chain
	debug::nextHandler(gui);

	// draw debug led's on gui
	gui.newLine();
	gui.led(debug::red ? 0x0000ff : 0);
	gui.led(debug::green ? 0x00ff00 : 0);
	gui.led(debug::blue ? 0xff0000 : 0);
}

void init() {
	// add to event loop handler chain
	debug::nextHandler = loop::addHandler(handle);
}

void setRedLed(bool value) {
	set(debug::red, value);
}

void toggleRedLed() {
	set(debug::red, !debug::red);
}

void setGreenLed(bool value) {
	set(debug::green, value);
}

void toggleGreenLed() {
	set(debug::green, !debug::green);
}

void setBlueLed(bool value) {
	set(debug::blue, value);
}

void toggleBlueLed() {
	set(debug::blue, !debug::blue);
}

} // namespace debug
