#include <timer.hpp>
#include <spi.hpp>
#include <loop.hpp>
#include <debug.hpp>


uint8_t spiData[] = {0x0f};

void transferSpi() {
	spi::transfer(SPI_CS1_PIN, spiData, 1, nullptr, 0, []() {transferSpi();});
}

bool blink = false;

int main(void) {
	loop::init();
	spi::init();
	debug::init();

	transferSpi();
	
	loop::run();
}
