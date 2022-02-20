#include <Spi.hpp>
#include <Loop.hpp>
#include <Debug.hpp>


uint8_t spiData[] = {0x00, 0xff, 0x0f, 0x55};

Coroutine transferSpi() {
	while (true) {
		co_await Spi::transfer(0, 4, spiData, 0, nullptr);
	}
}

int main(void) {
	Loop::init();
	Spi::init();
	Output::init(); // for debug led's

	transferSpi();
	
	Loop::run();
}
