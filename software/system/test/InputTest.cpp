#include <Input.hpp>
#include <Debug.hpp>
#include <Loop.hpp>
#include <boardConfig.hpp>


Coroutine handleInput() {
	while (true) {
		int index;
		bool value;

		// wait until trigger detected on input
		co_await Input::trigger(1 << INPUT_POTI_BUTTON, 1 << INPUT_PCB_BUTTON, index, value);
		if (index == 0) {
			// rising edge on poti button detected
			debug::toggleRed();
		} else {
			// falling edge on pcb button detected
			debug::toggleGreen();
		}
	}
}

int main() {
	loop::init();
	Output::init(); // for debug signals on pins
	Input::init();
	//Drivers drivers;

	handleInput();

	loop::run();
}
