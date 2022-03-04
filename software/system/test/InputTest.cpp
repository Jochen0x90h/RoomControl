#include <Input.hpp>
#include <Debug.hpp>
#include <Loop.hpp>


Coroutine handleInput() {
	while (true) {
		int index;
		bool value;

		// wait until trigger detected on input
		co_await Input::trigger(1 << INPUT_POTI_BUTTON, 1 << INPUT_PCB_BUTTON, index, value);
		if (index == 0) {
			// rising edge on poti button detected
			Debug::toggleRedLed();
			Debug::setGreenLed(false);
			Debug::toggleBlueLed();
		} else {
			// falling edge on pcb button detected
			Debug::setRedLed(false);
			Debug::toggleGreenLed();
			Debug::setBlueLed(false);
		}
	}
}

int main(void) {
	Loop::init();
	Output::init(); // for debug signals on pins
	Input::init();

	handleInput();

	Loop::run();
}
