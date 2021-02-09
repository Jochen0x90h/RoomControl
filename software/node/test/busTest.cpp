#include <timer.hpp>
#include <calendar.hpp>
#include <bus.hpp>
#include <debug.hpp>


uint8_t send[] = {0x55, 0x01, 0x33, 0x55, 0x00, 0x01, 0x33, 0x55};


void transferBus() {
	//bus::transferBus(send, 2, receive, 2, []() {transferBus();});
	bus::transferBus(send, 5, [](uint8_t const* data, int length) {debug::setRedLed(true);});
}

bool blink = false;

int main(void) {
	timer::init();
	calendar::init();
	bus::init();
	debug::init();

	//transferBus();
	
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
