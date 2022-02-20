#include <Spi.hpp>
#include <Debug.hpp>
#include <Loop.hpp>


uint8_t displayData[] = {
	0x00, 0xff,
	0x0f, 0x33, 0x55};

Coroutine writeDisplay() {
	while (true) {
		co_await Spi::writeCommand(SPI_DISPLAY, 2, displayData);
		co_await Spi::transfer(SPI_DISPLAY, 3, displayData + 2, 0, nullptr);
	}
}

uint8_t spiData[] = {0x0f, 0x7f, 0x00};

Coroutine transferSpi() {
	while (true) {
		co_await Spi::transfer(SPI_AIR_SENSOR, 3, spiData, 0, nullptr);
	}
}

int main(void) {
	Loop::init();
	Spi::init();
	Output::init(); // for debug led's

	writeDisplay();
	transferSpi();

	Loop::run();
}
