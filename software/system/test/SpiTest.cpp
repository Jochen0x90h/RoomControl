#include <Loop.hpp>
#include <Timer.hpp>
#include <Spi.hpp>
#include <Debug.hpp>


uint16_t spi0Data[] = {0x0a51, 0x0ff0};

Coroutine transferSpi0() {
	while (true) {
		co_await Spi::transfer(0, 2, spi0Data, 0, nullptr);
		//co_await Timer::sleep(100ms);
		//Debug::toggleRedLed();
	}
}


uint16_t spi1Command[] = {0x00ff, 0x3355};
uint16_t spi1Data[] = {0x3355, 0x00ff};

Coroutine transferSpi1() {
	while (true) {
		co_await Spi::writeCommand(1, 2, spi1Command);
		co_await Spi::transfer(1, 2, spi1Data, 0, nullptr);
	}
}



int main(void) {
	Loop::init();
	Output::init(); // for debug led's
	Timer::init();
	Spi::init();

	transferSpi0();
	transferSpi1();
	
	Loop::run();
}
