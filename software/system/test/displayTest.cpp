#include <spi.hpp>
#include <display.hpp>
#include <debug.hpp>
#include <loop.hpp>


uint8_t displayData[] = {
	0x00, 0xff,
	0x0f, 0x33, 0x55};

Coroutine writeDisplay() {
	while (true) {
		co_await display::send(2, 3, displayData);
	}
}

uint8_t spiData[] = {0x0f, 0x7f, 0x00};

Coroutine transferSpi() {
	while (true) {
		co_await spi::transfer(0, 3, spiData, 0, nullptr);
	}
}

int main(void) {
	loop::init();
	spi::init();
	display::init();
	out::init();

	writeDisplay();
	transferSpi();

	loop::run();
}
