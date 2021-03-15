#include <timer.hpp>
#include <bus.hpp>
#include <debug.hpp>
#include <util.hpp>


uint8_t send[] = {0x55, 0x01, 0x33, 0x55, 0x00, 0x01, 0x33, 0x55};
uint8_t receive[10];

void transferBus() {
	bus::transfer(send, 5, receive, array::size(receive), [](int length) {debug::setRedLed(true);});

	timer::start(0, timer::getTime() + 1s, []() {transferBus();});	
	debug::toggleBlueLed();
}

int main(void) {
	timer::init();
	bus::init();
	debug::init();
	
	transferBus();
		
	while (true) {
		timer::handle();
		bus::handle();
	}
}
