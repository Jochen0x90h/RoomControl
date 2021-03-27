#include <timer.hpp>
#include <debug.hpp>
#include <loop.hpp>


uint8_t timer1;
uint8_t timer2;
uint8_t timer3;

void setTimer1() {
	debug::toggleRedLed();
	timer::start(timer1, timer::getTime() + 1s, []() {setTimer1();});
}

void setTimer2() {
	debug::toggleGreenLed();
	timer::start(timer2, timer::getTime() + 5s + 500ms, []() {setTimer2();});
}

void setTimer3() {
	debug::toggleBlueLed();
	timer::start(timer3, timer::getTime() + 3s + 333ms, []() {setTimer3();});
}

int main(void) {
	loop::init();
	timer::init();
	debug::init();

	timer1 = timer::allocate();
	timer2 = timer::allocate();
	timer3 = timer::allocate();

	setTimer1();
	setTimer2();
	setTimer3();

	loop::run();
}
