#include <timer.hpp>
#include <calendar.hpp>
#include <debug.hpp>


int main(void) {
	timer::init();
	calendar::init();
	debug::init();
	
	calendar::addSecondHandler([]() {debug::toggleBlueLed();});
	
	while (true) {
		timer::handle();
		calendar::handle();
	}
}
