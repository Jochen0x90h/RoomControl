#include "../debug.hpp"
#include "loop.hpp"
#include <stdio.h>


namespace debug {

bool red = false;
bool green = false;
bool blue = false;

bool changed = false;

void print() {
	printf(debug::red ? "📕" : " ");
	printf(debug::green ? "📗" : " ");
	printf(debug::blue ? "📘" : " ");
	printf("\n");
	debug::changed = false;
}

void set(bool &value, bool newValue) {
	if (value != newValue && !debug::changed) {
		debug::changed = true;
		loop::context.post([]() {print();});
	}
	value = newValue;
}

void init() {}

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
