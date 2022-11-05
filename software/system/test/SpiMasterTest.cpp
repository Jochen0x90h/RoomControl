#include <Loop.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <boardConfig.hpp>


uint16_t spi0Data[] = {0x0a51, 0x0ff0};

Coroutine transferSpi0(SpiMaster &spi) {
	while (true) {
		co_await spi.transfer(2, spi0Data, 0, nullptr);
		//co_await Timer::sleep(100ms);
		//Debug::toggleRedLed();
	}
}


uint16_t spi1Command[] = {0x00ff, 0x3355};
uint16_t spi1Data[] = {0x3355, 0x00ff};

Coroutine transferSpi1(SpiMaster &spi) {
	while (true) {
		co_await spi.writeCommand(2, spi1Command);
		co_await spi.writeData(2, spi1Data);
	}
}


int main() {
	Loop::init();
	Output::init(); // for debug led's
	Timer::init();
	Drivers drivers;

	transferSpi0(drivers.airSensor);
	transferSpi1(drivers.display);
	
	Loop::run();
}
