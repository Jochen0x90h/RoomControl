#include <Timer.hpp>
#include <Loop.hpp>
#include <Debug.hpp>


Coroutine timer1() {
	while (true) {
		Debug::setRedLed(true);
		co_await Timer::sleep(100ms);
		
		Debug::setRedLed(false);
		co_await Timer::sleep(1900ms);
	}
}

Coroutine timer2() {
	while (true) {
		Debug::toggleGreenLed();
		co_await Timer::sleep(3s);
	}
}

Coroutine timer3() {
	while (true) {
		Debug::toggleBlueLed();
		co_await Timer::sleep(5s);
	}
}

int main(void) {
	Loop::init();
	Timer::init();
	Output::init(); // for debug signals on pins

	timer1();
	timer2();
	timer3();

	Loop::run();
}
