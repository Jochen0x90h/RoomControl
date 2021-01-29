#include <system.hpp>
#include <clock.hpp>
#include <debug.hpp>


bool blink0 = false;
bool blink1 = false;
bool blink2 = false;

void setTimer0() {
	debug::setRedLed(blink0 = !blink0);
	system::setTimer(0, system::getTime() + 1s, []() {setTimer0();});	
}

void setTimer1() {
	debug::setGreenLed(blink1 = !blink1);
	system::setTimer(1, system::getTime() + 5s + 500ms, []() {setTimer1();});	
}

void setTimer2() {
	debug::setBlueLed(blink2 = !blink2);
	system::setTimer(2, system::getTime() + 3s + 333ms, []() {setTimer2();});	
}

int main(void) {
	system::init();
	clock::init();
	debug::init();
	
	setTimer0();
	setTimer1();
	setTimer2();
	
	while (true) {
		system::handle();
		clock::handle();
	}
}
