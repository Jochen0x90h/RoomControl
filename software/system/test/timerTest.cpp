#include <timer.hpp>
#include <loop.hpp>
#include <debug.hpp>


Coroutine timer1() {
	while (true) {
		debug::setRedLed(true);
		co_await timer::sleep(100ms);
		
		debug::setRedLed(false);
		co_await timer::sleep(1900ms);
	}
}

Coroutine timer2() {
	while (true) {
		debug::toggleGreenLed();
		co_await timer::sleep(3s);
	}
}

Coroutine timer3() {
	while (true) {
		debug::toggleBlueLed();
		co_await timer::sleep(5s);
	}
}

int main(void) {
	loop::init();
	timer::init();
	gpio::init(); // for debug signals on pins

	timer1();
	timer2();
	timer3();

	loop::run();
}
