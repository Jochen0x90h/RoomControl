#include <spi.hpp>
#include <loop.hpp>
#include <debug.hpp>


uint8_t spiData[] = {0x00, 0xff, 0x0f, 0x55};

Coroutine transferSpi() {
	while (true) {
		co_await spi::transfer(0, 4, spiData, 0, nullptr);
	}
}

int main(void) {
	loop::init();
	spi::init();
	debug::init();

	transferSpi();
	
	loop::run();
}
