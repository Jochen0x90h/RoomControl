#include <calendar.hpp>
#include <debug.hpp>
#include <loop.hpp>


int main(void) {
	loop::init();
	calendar::init();
	debug::init();
	
	calendar::addSecondHandler([]() {debug::toggleBlueLed();});

	loop::run();	
}
