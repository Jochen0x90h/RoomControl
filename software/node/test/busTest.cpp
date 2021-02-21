#include <timer.hpp>
#include <calendar.hpp>
#include <bus.hpp>
#include <debug.hpp>
#include <util.hpp>


uint8_t send[] = {0x55, 0x01, 0x33, 0x55, 0x00, 0x01, 0x33, 0x55};
uint8_t receive[10];

void transferBus() {
	bus::transfer(send, 5, receive, array::size(receive), [](int length) {debug::setRedLed(true);});
}

bool blink = false;

int main(void) {
	timer::init();
	calendar::init();
	bus::init();
	debug::init();
	
	calendar::setSecondTick([]() {
		debug::setBlueLed(blink = !blink);
		transferBus();
	});
	
	while (true) {
		timer::handle();
		calendar::handle();
		bus::handle();
	}
}
