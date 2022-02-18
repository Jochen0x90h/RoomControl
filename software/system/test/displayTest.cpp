#include <spi.hpp>
#include <debug.hpp>
#include <loop.hpp>


uint8_t displayData[] = {
	0x00, 0xff,
	0x0f, 0x33, 0x55};

Coroutine writeDisplay() {
	while (true) {
		co_await spi::write(SPI_DISPLAY, 2, displayData, true);
		co_await spi::write(SPI_DISPLAY, 3, displayData + 2, false);
	}
}

uint8_t spiData[] = {0x0f, 0x7f, 0x00};

Coroutine transferSpi() {
	while (true) {
		co_await spi::transfer(SPI_AIR_SENSOR, 3, spiData, 0, nullptr);
	}
}

int main(void) {
	loop::init();
	spi::init();
	output::init(); // for debug led's

	writeDisplay();
	transferSpi();

	loop::run();
}
