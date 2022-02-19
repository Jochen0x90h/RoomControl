#include <Calendar.hpp>
#include <Debug.hpp>
#include <Loop.hpp>


Coroutine handleSecondTick() {
	while (true) {
		co_await Calendar::secondTick();
		Debug::toggleBlueLed();

		ClockTime time = Calendar::now();
		Debug::setRedLed((time.getMinutes() & 1) != 0);
		Debug::setGreenLed((time.getHours() & 1) != 0);
	}
}

int main(void) {
	Loop::init();
	Calendar::init();
	Output::init(); // for debug led's

	handleSecondTick();

	Loop::run();
}
