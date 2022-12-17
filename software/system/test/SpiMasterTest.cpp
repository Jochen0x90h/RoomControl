#include <Loop.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <boardConfig.hpp>


uint8_t spiWriteData[] = {0x0a, 0x55};
uint8_t spiReadData[10];

Coroutine transferSpi(SpiMaster &spi) {
	while (true) {
		co_await spi.transfer(2, spiWriteData, 10, spiReadData);
		//co_await Timer::sleep(100ms);
		//Debug::toggleRedLed();
	}
}


uint8_t command[] = {0x00, 0xff};
uint8_t data[] = {0x33, 0x55};

struct Spi {
	SpiMaster &command;
	SpiMaster &data;
};

Coroutine writeCommandData(Spi spi) {
	while (true) {
		co_await spi.command.write(2, command);
		co_await spi.data.write(2, data);
	}
}


int main() {
	Loop::init();
	Output::init(); // for debug led's
	Timer::init();
	DriversSpiMasterTest drivers;

	transferSpi(drivers.transfer);
	writeCommandData({drivers.command, drivers.data});
	
	Loop::run();
}
