#include <timer.hpp>
#include <spi.hpp>
#include <display.hpp>
#include <debug.hpp>
#include <loop.hpp>


uint8_t displayCommand[] = {0x55, 0x55};
uint8_t displayData[] = {0x33, 0x33};

void writeDisplay() {

	display::send(displayCommand, 1, 1, []() {});
	display::send(displayData, 1, 1, []() {writeDisplay();});
}

uint8_t spiData[] = {0x0f, 0x7f, 0x00};

void transferSpi() {
	spi::transfer(SPI_CS1_PIN, spiData, 3, nullptr, 0, []() {transferSpi();});
}

int main(void) {
	spi::init();
	display::init();
	debug::init();

	writeDisplay();
	transferSpi();

	loop::run();		
}
