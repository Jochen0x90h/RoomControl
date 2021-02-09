#include <timer.hpp>
#include <poti.hpp>
#include <debug.hpp>


bool blink = false;

int main(void) {
	timer::init();
	poti::init();
	debug::init();	
	
	poti::setHandler([](int d, bool activated) {
		debug::setRedLed(d & 1);
		debug::setGreenLed(d & 2);
		debug::setBlueLed(d & 4);
		if (activated)
			debug::setGreenLed(blink = !blink);		
	});	
			
	while (true) {
		timer::handle();
		poti::handle();
	}
}
