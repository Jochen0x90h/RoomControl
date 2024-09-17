#include <BusMaster.hpp>
#include <Loop.hpp>
#include <Debug.hpp>
#include <util.hpp>
#include <boardConfig.hpp>


uint8_t send[] = {0x01, 0x00, 0x0f, 0x33, 0x55, 0xaa, 0xcc, 0xf0};
uint8_t receive[10];


Coroutine transferBus(BusMaster &busMaster) {
	while (true) {
		co_await busMaster.send(array::count(send), send);

		//int receiveLength = array::count(receive);

		co_await loop::sleep(1s);
		
		//Debug::toggleBlueLed();
	}
}

int main() {
	loop::init();
	Output::init(); // for debug led's
	DriversBusMasterTest drivers;

	transferBus(drivers.busMaster);

	loop::run();
}
