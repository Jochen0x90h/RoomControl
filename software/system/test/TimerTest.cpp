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

		auto timeout = Timer::now() + 3s;
		co_await Timer::sleep(timeout);

		// test if sleep with elapsed timeout works
		co_await Timer::sleep(timeout);
	}
}

Coroutine timer3() {
	while (true) {
		Debug::toggleBlueLed();

		// test if time overflow works on nrf52
		auto time = Timer::now();
		int i = (time.value >> 20) & 3;

		co_await Timer::sleep(500ms + i * 1s);
	}
}

int main(void) {
	Loop::init();
	Output::init(); // for debug signals on pins
	Timer::init();

	timer1();
	timer2();
	timer3();

	Loop::run();
}
