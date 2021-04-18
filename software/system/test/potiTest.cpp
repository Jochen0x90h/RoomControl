#include <poti.hpp>
#include <timer.hpp>
#include <debug.hpp>
#include <loop.hpp>


int main(void) {
	loop::init();
	timer::init();
	poti::init();
	debug::init();	
	
	poti::setHandler([](int d, bool activated) {
		if (!activated) {
			debug::setRedLed(d & 1);
			debug::setGreenLed(d & 2);
			debug::setBlueLed(d & 4);
		} else {
			debug::setRedLed(false);
			debug::toggleGreenLed();
			debug::setBlueLed(false);
		}
	});	

	loop::run();	
}
