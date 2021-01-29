#include <system.hpp>
#include <clock.hpp>
#include <poti.hpp>
#include <spi.hpp>
#include <debug.hpp>


uint8_t command[] = {0x55, 0x55};
uint8_t data[] = {0x33, 0x33};

void writeDisplay() {

	spi::writeDisplay(command, 1, 1, []() {});
	spi::writeDisplay(data, 1, 1, []() {writeDisplay();});
	
	/*spi::writeDisplay(command, 1, 1, []() {
		spi::writeDisplay(data, 1, 1, []() {writeDisplay();});
	});*/
}

uint8_t airSensor[] = {0x0f};

void transferAirSensor() {
	spi::transferAirSensor(airSensor, 1, nullptr, 0, []() {transferAirSensor();});
}

bool blink = false;

int main(void) {
	system::init();
	spi::init();
	debug::init();

	writeDisplay();
	transferAirSensor();
		
	while (true) {
		system::handle();
		spi::handle();
	}
}
