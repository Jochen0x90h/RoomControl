#include <system.hpp>
#include <clock.hpp>
#include <bus.hpp>
#include <debug.hpp>


uint8_t send[] = {0x55, 0x01};
uint8_t receive[10];


void transferBus() {
	bus::transferBus(send, 2, receive, 2, []() {transferBus();});
}

bool blink = false;

int main(void) {
	system::init();
	clock::init();
	bus::init();
	debug::init();

	transferBus();
	
	clock::setSecondTick([]() {
		debug::setBlueLed(blink = !blink);
	});
	
	while (true) {
		system::handle();
		clock::handle();
		bus::handle();
	}
}
