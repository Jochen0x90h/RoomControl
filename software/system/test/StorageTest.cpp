#include <Loop.hpp>
#include <Timer.hpp>
#include <Storage.hpp>
#include <Debug.hpp>
#include <Coroutine.hpp>


uint8_t writeData[] = {0x12, 0x34, 0x56, 0x78};
uint8_t readData[4];


bool compare() {
	for (int i = 0; i < 4; ++i) {
		if (readData[i] != writeData[i]) {
			// data is different!
			Debug::setRedLed();
			Debug::clearGreenLed();
			return false;
		}
	}
	Debug::clearRedLed();
	Debug::setGreenLed();
	return true;
}

Coroutine store() {
	// read from last run
	Storage2::read(0, 5, 4, readData);
	compare();

	co_await Timer::sleep(1s);

	Storage2::write(0, 5, 4, writeData);
	Storage2::read(0, 5, 4, readData);
	compare();

	while (true) {
		co_await Timer::sleep(1s);
	}
}


int main(void) {
	Loop::init();
	Timer::init();
	Storage2::init();
	Output::init(); // for debug led's

	store();

	Loop::run();
}
