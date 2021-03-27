#include <timer.hpp>
#include <bus.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <util.hpp>


uint8_t timerId;

uint8_t send[] = {0x55, 0x01, 0x33, 0x55, 0x00, 0x01, 0x33, 0x55};
uint8_t receive[10];


void transferBus() {
	bus::transfer(send, 5, receive, array::size(receive), [](int length) {debug::toggleRedLed();});

	timer::start(timerId, timer::getTime() + 1s, []() {transferBus();});
	debug::toggleBlueLed();
}

int main(void) {
	loop::init();
	timer::init();
	bus::init();
	debug::init();

	timerId = timer::allocate();

	transferBus();

	loop::run();
}
