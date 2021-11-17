#include <timer.hpp>
#include <bus.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <util.hpp>


uint8_t send[] = {0x01, 0x00, 0x0f, 0x33, 0x55, 0xaa, 0xcc, 0xf0};
uint8_t receive[10];


Coroutine transferBus() {
	while (true) {
		int receiveLength = array::size(receive);
		co_await bus::transfer(array::size(send), send, receiveLength, receive);
		
		//bus::transfer(send, 5, receive, array::size(receive), [](int length) {debug::toggleRedLed();});
	
		//timer::start(0, timer::now() + 1s, []() {transferBus();});
		co_await timer::delay(1s);
		
		debug::toggleBlueLed();
	}
}

int main(void) {
	loop::init();
	timer::init();
	bus::init();
	out::init();

	transferBus();

	loop::run();
}
