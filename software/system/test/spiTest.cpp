#include <spi.hpp>
#include <loop.hpp>
#include <debug.hpp>


uint8_t spiData[] = {0x0f};

void transferSpi() {
	spi::transfer(0, spiData, 1, nullptr, 0, []() {transferSpi();});
}

int main(void) {
	loop::init();
	spi::init();
	debug::init();

	transferSpi();
	
	loop::run();
}
