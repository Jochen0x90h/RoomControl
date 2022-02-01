#include <poti.hpp>
#include <timer.hpp>
#include <debug.hpp>
#include <loop.hpp>


Coroutine handlePoti() {
	while (true) {
		int8_t d;
		bool activated;
		
		// wait until poti has changed for up to 2 seconds
		switch (co_await select(poti::change(d, activated), timer::sleep(2s))) {
		case 1:
			if (!activated) {
				debug::setRedLed(d & 1);
				debug::setGreenLed(d & 2);
				debug::setBlueLed(d & 4);
			} else {
				debug::setRedLed(false);
				debug::toggleGreenLed();
				debug::setBlueLed(false);
			}
			break;
		case 2:
			debug::toggleBlueLed();
			break;
		}		
	}
}

int main(void) {
	loop::init();
	timer::init();
	poti::init();
	gpio::init(); // for debug signals on pins

	handlePoti();

	loop::run();	
}
