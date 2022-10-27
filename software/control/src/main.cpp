#include "RoomControl.hpp"
#include <Calendar.hpp>
#include <Radio.hpp>
#include <QuadratureDecoder.hpp>
#include <Input.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <Loop.hpp>
#include <Sound.hpp>


/**
 * Emulator main, start without parameters
 */
int main(int argc, const char **argv) {
	// init drivers
	Loop::init();
	Timer::init();
	Calendar::init();
	Radio::init();
	Network::init();
	Output::init();
	Input::init();
	Sound::init();
	Drivers drivers;

	Radio::start(15); // start on channel 15
	Radio::enableReceiver(true); // enable baseband decoder

	// set default configuration
	Configuration configuration;
	int configurationSize = sizeof(Configuration);
	drivers.storage.readBlocking(STORAGE_ID_CONFIG, configurationSize, &configuration);
	if (configurationSize != sizeof(Configuration)) {
		static uint8_t const networkKey[] = {0xe6, 0x63, 0x2b, 0xa3, 0x55, 0xd4, 0x76, 0x82, 0x63, 0xe8, 0xb5, 0x9a, 0x2a, 0x6b, 0x41, 0x44};
		configuration.key.setData(0, 16, networkKey);
		setKey(configuration.aesKey, configuration.key);
		configuration.ieeeLongAddress = UINT64_C(0x00124b00214f3c55);
		configuration.zbeePanId = 0x1a62;
		configuration.mqttLocalPort = 1337;
		configuration.mqttGateway = {Network::Address::fromString("::1"), 10000};
		configuration.wheelPlugCount = 1;

		drivers.storage.writeBlocking(STORAGE_ID_CONFIG, sizeof(Configuration), &configuration);
	}

	// the room control application
	RoomControl roomControl(drivers);

	Loop::run();

	return 0;
}
