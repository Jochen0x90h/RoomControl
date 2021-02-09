#include <timer.hpp>
#include <calendar.hpp>
#include <debug.hpp>


bool blink0 = false;
bool blink1 = false;
bool blink2 = false;

void setTimer0() {
	debug::setRedLed(blink0 = !blink0);
	timer::start(0, timer::getTime() + 1s, []() {setTimer0();});	
}

void setTimer1() {
	debug::setGreenLed(blink1 = !blink1);
	timer::start(1, timer::getTime() + 5s + 500ms, []() {setTimer1();});	
}

void setTimer2() {
	debug::setBlueLed(blink2 = !blink2);
	timer::start(2, timer::getTime() + 3s + 333ms, []() {setTimer2();});	
}

int main(void) {
	timer::init();
	calendar::init();
	debug::init();
	
	setTimer0();
	setTimer1();
	setTimer2();
	
	while (true) {
		timer::handle();
		calendar::handle();
	}
}
