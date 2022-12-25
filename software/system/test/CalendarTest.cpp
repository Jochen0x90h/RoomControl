#include <Calendar.hpp>
#include <Debug.hpp>
#include <Loop.hpp>


Coroutine handleSecondTick() {
	while (true) {
		co_await Calendar::secondTick();
		debug::toggleBlue();

		ClockTime time = Calendar::now();
		debug::setRed((time.getMinutes() & 1) != 0);
		debug::setGreen((time.getHours() & 1) != 0);
	}
}

int main() {
	loop::init();
	Calendar::init();
	Output::init(); // for debug led's

	handleSecondTick();

	loop::run();
}
