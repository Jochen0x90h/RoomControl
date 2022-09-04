#include <Timer.hpp>
#include <BusMaster.hpp>
#include <Loop.hpp>
#include <Debug.hpp>
#include <util.hpp>


uint8_t send[] = {0x01, 0x00, 0x0f, 0x33, 0x55, 0xaa, 0xcc, 0xf0};
uint8_t receive[10];


Coroutine transferBus() {
	while (true) {
		co_await BusMaster::send(array::count(send), send);

		//int receiveLength = array::count(receive);

		co_await Timer::sleep(1s);
		
		Debug::toggleBlueLed();
	}
}

int main(void) {
	Loop::init();
	Timer::init();
	BusMaster::init();
	Output::init(); // for debug led's

	transferBus();

	Loop::run();
}
