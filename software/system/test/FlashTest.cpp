#include <Loop.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <boardConfig.hpp>


uint8_t writeData[] = {0x12, 0x34, 0x56, 0x78};
uint8_t readData[4];


int main(void) {
	Loop::init();
	Timer::init();
	Output::init(); // for debug led's
	DriversFlashTest drivers;

	drivers.flash.readBlocking(0, 4, readData);
	if (array::equal(4, writeData, readData)) {
		// blue indicates that the data is there from the last run
		Debug::setBlueLed();

		// erase
		drivers.flash.eraseSectorBlocking(0);

		while (true) {}
	}

	// erase
	drivers.flash.eraseSectorBlocking(0);

	// write data
	drivers.flash.writeBlocking(0, 4, writeData);

	// read data and check if equal
	drivers.flash.readBlocking(0, 4, readData);
	if (array::equal(4, writeData, readData)) {
		// green indicates that write and read was successful
		Debug::setGreenLed();
	} else {
		// read indicates that write or read failed
		Debug::setRedLed();
	}

	while (true) {}
}
