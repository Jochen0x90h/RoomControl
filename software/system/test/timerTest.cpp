#include <timer.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <Coroutine.hpp>


Coroutine timer1() {
	while (true) {
		debug::setRedLed(true);
		co_await timer::delay(100ms);
		
		debug::setRedLed(false);
		co_await timer::delay(1900ms);
	}
}

Coroutine timer2() {
	while (true) {
		debug::toggleGreenLed();
		co_await timer::delay(3s);
	}
}

Coroutine timer3() {
	while (true) {
		debug::toggleBlueLed();
		co_await timer::delay(5s);	
	}
}

int main(void) {
	loop::init();
	timer::init();
	out::init();

	timer1();
	timer2();
	timer3();

	loop::run();
}
