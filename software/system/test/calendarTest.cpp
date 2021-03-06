#include <calendar.hpp>
#include <debug.hpp>
#include <loop.hpp>


Coroutine handleSecondTick() {
	while (true) {
		co_await calendar::secondTick();
		debug::toggleBlueLed();
	}
}

int main(void) {
	loop::init();
	calendar::init();
	debug::init();
	
	handleSecondTick();

	loop::run();	
}
