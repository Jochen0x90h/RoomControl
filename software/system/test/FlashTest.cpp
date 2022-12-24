#include <Loop.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <boardConfig.hpp>


uint8_t writeData[] = {0x12, 0x34, 0x56, 0x78};
uint8_t readData[4];


void test() {
	DriversFlashTest drivers;

	drivers.flash.readBlocking(0, 4, readData);
	if (array::equal(4, writeData, readData)) {
		// blue indicates that the data is there from the last run
		debug::setBlue();

		// erase
		drivers.flash.eraseSectorBlocking(0);

		// also switch on red and green leds in case erase did not work
		drivers.flash.readBlocking(0, 4, readData);
		if (array::equal(4, writeData, readData)) {
			debug::setRed();
			debug::setGreen();
		}

		return;
	}

	// erase
	drivers.flash.eraseSectorBlocking(0);

	// write data
	drivers.flash.writeBlocking(0, 4, writeData);

	// read data and check if equal
	drivers.flash.readBlocking(0, 4, readData);
	if (array::equal(4, writeData, readData)) {
		// green indicates that write and read was successful
		debug::setGreen();
	} else {
		// read indicates that write or read failed
		debug::setRed();
	}
}

int main() {
	Loop::init();
	Timer::init();
	Output::init(); // for debug led's

	test();

	Loop::run();
}
