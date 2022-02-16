#include <calendar.hpp>
#include <debug.hpp>
#include <loop.hpp>


Coroutine handleSecondTick() {
	while (true) {
		co_await calendar::secondTick();
		debug::toggleBlueLed();

		ClockTime time = calendar::now();
		debug::setRedLed((time.getMinutes() & 1) != 0);
		debug::setGreenLed((time.getHours() & 1) != 0);
	}
}

int main(void) {
	loop::init();
	calendar::init();
	output::init(); // for debug led's

	handleSecondTick();

	loop::run();
}
