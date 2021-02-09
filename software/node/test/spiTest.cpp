#include <timer.hpp>
#include <spi.hpp>
#include <debug.hpp>


uint8_t displayCommand[] = {0x55, 0x55};
uint8_t displayData[] = {0x33, 0x33};

void writeDisplay() {

	spi::writeDisplay(displayCommand, 1, 1, []() {});
	spi::writeDisplay(displayData, 1, 1, []() {writeDisplay();});
	
	/*spi::writeDisplay(command, 1, 1, []() {
		spi::writeDisplay(data, 1, 1, []() {writeDisplay();});
	});*/
}

uint8_t spiData[] = {0x0f};

void transferSpi() {
	spi::transfer(SPI_CS_PIN, spiData, 1, nullptr, 0, []() {transferSpi();});
}

bool blink = false;

int main(void) {
	timer::init();
	spi::init();
	debug::init();

	writeDisplay();
	transferSpi();
		
	while (true) {
		timer::handle();
		spi::handle();
	}
}
