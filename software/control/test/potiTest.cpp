#include <system.hpp>
#include <clock.hpp>
#include <poti.hpp>
#include <debug.hpp>


bool blink = false;

int main(void) {
	system::init();
	poti::init();
	debug::init();	
	
	poti::setPotiHandler([](int d) {
		debug::setRedLed(d & 1);
		debug::setGreenLed(d & 2);
		debug::setBlueLed(d & 4);
	});	
	
	poti::setButtonHandler([]() {
		debug::setGreenLed(blink = !blink);		
	});
		
	while (true) {
		system::handle();
		poti::handle();
	}
}
