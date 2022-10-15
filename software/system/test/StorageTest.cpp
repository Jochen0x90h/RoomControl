#include <Loop.hpp>
#include <Timer.hpp>
#include <Storage.hpp>
#include <Debug.hpp>
#include <Coroutine.hpp>
#include <boardConfig.hpp>


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

Coroutine store(Storage &storage) {
	// read from last run
	int size = 4;
	storage.readBlocking(5, size, readData);
	compare();

	co_await Timer::sleep(1s);

	storage.writeBlocking(5, 4, writeData);
	size = 4;
	storage.readBlocking(5, size, readData);
	compare();

	while (true) {
		co_await Timer::sleep(1s);
	}
}


int main(void) {
	Loop::init();
	Timer::init();
	Output::init(); // for debug led's
	Drivers drivers;

	store(drivers.storage);

	Loop::run();
}
