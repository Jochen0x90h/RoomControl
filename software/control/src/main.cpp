#include "RoomControl.hpp"
#include <Calendar.hpp>
#include <Flash.hpp>
#include <BusMaster.hpp>
#include <Radio.hpp>
#include <Poti.hpp>
#include <Input.hpp>
#include <Timer.hpp>
#include <Debug.hpp>
#include <Spi.hpp>
#include <Loop.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <iterator>


/*
struct DeviceData {
	uint32_t deviceId;
	String name;
	
	struct Component {
		RoomControl::Device::Component::Type type;
		uint8_t endpointIndex;
		uint8_t elementIndex;
		SystemDuration duration;
	};
	
	int componentCount;
	Component components[10];
};

// configuration for local devices (air sensor, brightness sensor, motion detector)
constexpr DeviceData localDeviceData[] = {
	{0x00000001, "tempsensor", 1, {
		{RoomControl::Device::Component::CELSIUS, 0, 0}
	}}
};


// configuration for emulated devices in bus.cpp
constexpr DeviceData busDeviceData[] = {
	{0x00000001, "switch1", 7, {
		{RoomControl::Device::Component::ROCKER, 0, 0},
		{RoomControl::Device::Component::BUTTON, 1, 0},
		{RoomControl::Device::Component::DELAY_BUTTON, 1, 0, 2s},
		{RoomControl::Device::Component::SWITCH, 1, 1},
		{RoomControl::Device::Component::RELAY, 2, 0},
		{RoomControl::Device::Component::TIME_RELAY, 3, 0, 3s},
		{RoomControl::Device::Component::RELAY, 4, 0}
	}},
	{0x00000002, "switch2", 10, {
		{RoomControl::Device::Component::ROCKER, 0, 0},
		{RoomControl::Device::Component::BUTTON, 1, 0},
		{RoomControl::Device::Component::BLIND, 2, 0, 6500ms},
		{RoomControl::Device::Component::HOLD_ROCKER, 3, 0, 2s},
		{RoomControl::Device::Component::HOLD_BUTTON, 4, 0, 2s},
		{RoomControl::Device::Component::BLIND, 5, 0, 6500ms},
		{RoomControl::Device::Component::SWITCH, 5, 0},
		{RoomControl::Device::Component::SWITCH, 5, 1},
		{RoomControl::Device::Component::RELAY, 6, 0},
		{RoomControl::Device::Component::RELAY, 7, 0}
	}},
	{0x00000003, "tempsensor", 1, {
		{RoomControl::Device::Component::CELSIUS, 0, 0}
	}}
};

void setDevices(Array<DeviceData const> deviceData, Storage::Array<RoomControl::Device, RoomControl::DeviceState> &devices) {
	for (auto d : deviceData) {
		RoomControl::Device device = {};
		RoomControl::DeviceState deviceState = {};
		device.deviceId = d.deviceId;
		device.setName(d.name);
		
		// add components
		for (int i = 0; i < d.componentCount; ++i) {
			auto &componentData = d.components[i];
			auto type = componentData.type;

			RoomControl::ComponentEditor editor(device, deviceState, i);
			auto &component = editor.insert(type);
			++device.componentCount;
			
			component.endpointIndex = componentData.endpointIndex;
			component.nameIndex = device.getNameIndex(type, i);
			component.elementIndex = componentData.elementIndex;
			
			if (component.is<RoomControl::Device::TimeComponent>()) {
				auto &timeComponent = component.cast<RoomControl::Device::TimeComponent>();
				timeComponent.duration = componentData.duration;
			}
		}
		devices.write(devices.size(), &device);
	}
}

constexpr String routeData[][2] = {
	{"room/switch1/rk0", "room/switch1/rl0"},
	{"room/switch1/bt0", "room/switch1/rl1"},
	{"room/switch1/bt1", "room/switch1/rl2"},
	{"room/switch1/sw0", "room/switch1/rl2"},
	{"room/switch2/rk0", "room/switch2/bl0"},
	{"room/switch2/bt0", "room/switch2/bl0"},
	{"room/switch2/rk1", "room/switch2/bl1"},
	{"room/switch2/bt1", "room/switch2/bl1"},
	{"room/switch2/sw0", "room/switch2/rl0"},
	{"room/switch2/sw1", "room/switch2/rl1"},
};

struct TimerData {
	ClockTime time;
	String topic;
};
constexpr TimerData timerData[] = {
	{ClockTime(1, 10, 00), "room/switch1/rl0"},
	{ClockTime(2, 22, 41), "room/switch1/rl1"}
};
*/

/**
 * Emulator main, start without parameters
 */
int main(int argc, const char **argv) {
	// init drivers
	Loop::init();
	Flash::init();
	Timer::init();
	Calendar::init();
	Spi::init();
	Poti::init();
	BusMaster::init();
	Radio::init();
	Output::init();
	Input::init();

	Radio::start(15); // start on channel 15
	Radio::enableReceiver(true); // enable baseband decoder

	// the room control application
	RoomControl roomControl;

	// set configuration
	ConfigurationFlash configuration = *roomControl.configurations[0];
	static uint8_t const networkKey[] = {0xe6, 0x63, 0x2b, 0xa3, 0x55, 0xd4, 0x76, 0x82, 0x63, 0xe8, 0xb5, 0x9a, 0x2a, 0x6b, 0x41, 0x44};
	configuration.key.setData(0, 16, networkKey);
	setKey(configuration.aesKey, configuration.key);
	configuration.ieeeLongAddress = UINT64_C(0x00124b00214f3c55);
	configuration.mqttLocalPort = 1337;
	configuration.mqttGateway = {Network::Address::fromString("::1"), 10000};
	roomControl.configurations.write(0, configuration);
	roomControl.applyConfiguration();
/*
	// add test data
	setDevices(localDeviceData, roomControl.localDevices);
	setDevices(busDeviceData, roomControl.busDevices);
	for (auto r : routeData) {
		RoomControl::Route route = {};
		route.setSrcTopic(r[0]);
		route.setDstTopic(r[1]);
		roomControl.routes.write(roomControl.routes.size(), &route);
	}
	for (auto t : timerData) {
		RoomControl::Timer timer = {};
		RoomControl::TimerState timerState = {};
		timer.time = t.time;
		
		// add commands
		{
			RoomControl::CommandEditor editor(timer, timerState);
			editor.insert();
			editor.setTopic(t.topic);
			editor.setValueType(RoomControl::Command::ValueType::SWITCH);
			++timer.commandCount;
			editor.next();
		}
		
		roomControl.timers.write(roomControl.timers.size(), &timer);
	}

	// debug print devices
	for (auto e : roomControl.busDevices) {
		RoomControl::Device const &device = *e.flash;
		RoomControl::DeviceState &deviceState = *e.ram;
		std::cout << device.getName() << std::endl;
		for (RoomControl::ComponentIterator it(device, deviceState); !it.atEnd(); it.next()) {
			auto &component = it.getComponent();
			StringBuffer<8> b = component.getName();
			std::cout << "\t" << b << std::endl;
		}
	}

	// debug print timers
	for (auto e : roomControl.timers) {
		RoomControl::Timer const &timer = *e.flash;
		RoomControl::TimerState &timerState = *e.ram;
		
		int minutes = timer.time.getMinutes();
		int hours = timer.time.getHours();
		StringBuffer<16> b = dec(hours) + ':' + dec(minutes, 2);
		std::cout << b << std::endl;
		for (RoomControl::CommandIterator it(timer, timerState); !it.atEnd(); it.next()) {
			auto &command = it.getCommand();
			std::cout << "\t" << it.getTopic() << std::endl;
			std::cout << "\t";
			switch (command.valueType) {
			case RoomControl::Command::ValueType::BUTTON:
				std::cout << "BUTTON";
				break;
			case RoomControl::Command::ValueType::SWITCH:
				std::cout << "SWITCH";
				break;
			}
			std::cout << std::endl;
		}
	}

	// subscribe devices after test data has been added
	//roomControl.subscribeInterface(roomControl.localInterface, roomControl.localDevices);
	roomControl.subscribeAll();
*/

	Loop::run();

	return 0;
}
