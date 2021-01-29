#include <system.hpp>
#include <poti.hpp>
#include <spi.hpp>
#include <clock.hpp>
#include <debug.hpp>


int const channel = 15;

uint8_t packet[200];

uint8_t command[] = {0x55, 0x55};
uint8_t data[] = {0x33, 0x33};

void writeDisplay() {

	spi::writeDisplay(command, 1, 1, []() {});
	spi::writeDisplay(data, 1, 1, []() {writeDisplay();});
	
	/*system::writeDisplay(command, 1, 1, []() {
		system::writeDisplay(data, 1, 1, []() {writeDisplay();});
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
	clock::init();
	/*
	system::setQdecHandler([](int d) {
		system::setRedLed(d & 1);
		system::setGreenLed(d & 2);
		system::setBlueLed(d & 4);
	});
	*/
	//writeDisplay();
	//transferAirSensor();
	
	clock::setSecondTick([]() {debug::setBlueLed(blink = !blink);});
	
	while (true) {
		system::handle();
		spi::handle();
		clock::handle();
	}
}
