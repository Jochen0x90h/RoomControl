#include <calendar.hpp>
#include <debug.hpp>
#include <loop.hpp>


int main(void) {
	loop::init();
	calendar::init();
	debug::init();
	
	calendar::setSecondHandler(0, []() {debug::toggleBlueLed();});

	loop::run();	
}
