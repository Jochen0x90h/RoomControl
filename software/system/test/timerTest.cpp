#include <timer.hpp>
#include <debug.hpp>


void setTimer0() {
	debug::toggleRedLed();
	timer::start(0, timer::getTime() + 1s, []() {setTimer0();});
}

void setTimer1() {
	debug::toggleGreenLed();
	timer::start(1, timer::getTime() + 5s + 500ms, []() {setTimer1();});
}

void setTimer2() {
	debug::toggleBlueLed();
	timer::start(2, timer::getTime() + 3s + 333ms, []() {setTimer2();});
}

int main(void) {
	timer::init();
	debug::init();
	
	setTimer0();
	setTimer1();
	setTimer2();
	
	while (true) {
		timer::handle();
	}
}
