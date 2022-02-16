#include <poti.hpp>
#include <input.hpp>
#include <timer.hpp>
#include <debug.hpp>
#include <loop.hpp>
#ifdef EMU
#include "terminal.hpp"
#include <StringOperators.hpp>
#endif


Coroutine handlePoti() {
	while (true) {
		int8_t delta;
		int index;
		bool value;

		// wait until poti has changed or rising edge on button input for up to 2 seconds
		switch (co_await select(poti::change(0, delta), input::trigger(1 << INPUT_POTI_BUTTON, 1 << INPUT_PCB_BUTTON, index, value), timer::sleep(2s))) {
		case 1:
#ifdef EMU
			terminal::out << "delta " << dec(delta) << '\n';
#endif
			debug::setRedLed(delta & 1);
			debug::setGreenLed(delta & 2);
			debug::setBlueLed(delta & 4);
			break;
		case 2:
#ifdef EMU
			terminal::out << "activated " << dec(index) << '\n';
#endif
			if (index == 0) {
				debug::toggleRedLed();
				debug::setGreenLed(false);
			} else {
				debug::setRedLed(false);
				debug::toggleGreenLed();
			}
			debug::setBlueLed(false);
			break;
		case 3:
			{
				// also test if time overflow works
				auto time = timer::now();
				int i = time.value >> 20;
				switch (i) {
				case 0:
					debug::toggleRedLed();
					debug::setGreenLed(false);
					debug::setBlueLed(false);
					break;
				case 1:
					debug::setRedLed(false);
					debug::toggleGreenLed();
					debug::setBlueLed(false);
					break;
				default:
					debug::setRedLed(false);
					debug::setGreenLed(false);
					debug::toggleBlueLed();
				}
			}
			break;
		}
	}
}

int main(void) {
	loop::init();
	timer::init();
	poti::init();
	output::init(); // for debug signals on pins
	input::init();

	handlePoti();

	loop::run();
}
