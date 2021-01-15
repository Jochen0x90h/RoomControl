#include "Bus.hpp"
#include "util.hpp"
#include <iostream>


// list of emulated devices
constexpr DeviceId deviceIds[] = {
	0x00000001,
	0x00000002,
	0x00000003
};

constexpr EndpointType device1Endpoints[] = {EndpointType::ROCKER, EndpointType::ROCKER, EndpointType::LIGHT, EndpointType::LIGHT, EndpointType::LIGHT};
constexpr EndpointType device2Endpoints[] = {EndpointType::ROCKER, EndpointType::BUTTON, EndpointType::BLIND, EndpointType::ROCKER, EndpointType::BUTTON, EndpointType::BLIND, EndpointType::LIGHT, EndpointType::LIGHT};
constexpr EndpointType device3Endpoints[] = {EndpointType::TEMPERATURE_SENSOR};

Bus::Bus() {
	// post onLinReady() into event loop
	boost::asio::post(global::context, [this] {
		onBusReady();
	});
	
	// clear states
	std::fill(std::begin(this->states), std::end(this->states), State{});
}

Bus::~Bus() {
}

Array<DeviceId> Bus::getBusDevices() {
	return deviceIds;
}

Array<EndpointType> Bus::getBusDeviceEndpoints(DeviceId deviceId) {
	switch (deviceId) {
	case 0x00000001:
		return device1Endpoints;
	case 0x00000002:
		return device2Endpoints;
	case 0x00000003:
		return device3Endpoints;
	}
	return {};
}

void Bus::subscribeBus(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
	int stateIndex = 0;
	for (DeviceId devId : deviceIds) {
		if (devId == deviceId) {
			auto &state = this->states[stateIndex + endpointIndex];
			if (state.endpointId != 0) {
				assert(endpointId == 0 || endpointId == state.endpointId);
			}
			endpointId = state.endpointId = stateIndex + endpointIndex + 1;
			return;
		}
		stateIndex += getBusDeviceEndpoints(devId).length;
	}
	return 0;
}

void Bus::unsubscribeBus(uint8_t &endpointId) {
	endpointId = 0;
}

void Bus::busSend(uint8_t endpointId, const uint8_t *data, int length) {
	assert(endpointId >= 1 && endpointId <= 60);
	assert(length <= sizeof(8));
	
	State &state = this->states[endpointId - 1];
	assert(state.endpointId == endpointId);
	state.relays = data[0];
	
	// emulate busy until next gui update
	// todo: decide if we need a busy flag or if there is a data cache for all endpoints
	this->sendBusy = true;
}

void Bus::doGui(int &id) {
	this->sendBusy = false;
	Gui *gui = global::gui;

	// time difference
	auto now = std::chrono::steady_clock::now();
	int ms = std::chrono::duration_cast<std::chrono::microseconds>(now - this->time).count();
	this->time = now;

	// iterate over devices
	State *it = this->states;
	for (auto deviceId : deviceIds) {
		// iterate over endpoints of device
		for (EndpointType type : getBusDeviceEndpoints(deviceId)) {
			State &state = *it++;
			
			// visualize only if endpoint is subscribed
			if (state.endpointId != 0) {
				switch (type) {
				case EndpointType::BINARY_SENSOR:
					break;
				case EndpointType::BUTTON:
					// button
					{
						int value = gui->button(id++);
						if (value != -1) {
							uint8_t data = uint8_t(value);
							onBusReceived(state.endpointId, &data, 1);
						}
					}
					break;
				case EndpointType::SWITCH:
					// switch
					{
						int value = gui->switcher(id++);
						if (value != -1) {
							uint8_t data = uint8_t(value);
							onBusReceived(state.endpointId, &data, 1);
						}
					}
					break;
				case EndpointType::ROCKER:
					// rocker
					{
						int value = gui->rocker(id++);
						if (value != -1) {
							uint8_t data = uint8_t(value);
							onBusReceived(state.endpointId, &data, 1);
						}
					}
					break;
				case EndpointType::RELAY:
					// relay
					break;
				case EndpointType::LIGHT:
					// light
					gui->light(state.relays & 1, 100);
					break;
				case EndpointType::BLIND:
					// blind
					if (state.relays & 1)
						state.blind = std::min(state.blind + ms, 100*65536);
					else if (state.relays & 2)
						state.blind = std::max(state.blind - ms, 0);
					gui->blind(state.blind >> 16);
					break;

/*
				case EndpointType::RELAY_2:
					// two relays
					if (deviceInfo.relayType == DeviceInfo::LIGHT) {
						// visualize as two lights
						gui->light(state.relays & 1, 100);
						gui->light(state.relays & 2, 100);
					} else {
						// visualize as blind
						if (state.relays & 1)
							state.blind1 = std::min(state.blind1 + ms, 100*65536);
						else if (state.relays & 2)
							state.blind1 = std::max(state.blind1 - ms, 0);
						gui->blind(state.blind1 >> 16);
					}
					break;
				case EndpointType::RELAY_4:
					// four relays
					if (deviceInfo.relayType == DeviceInfo::LIGHT) {
						// visualize as four lights
						gui->light(state.relays & 1, 100);
						gui->light(state.relays & 2, 100);
						gui->light(state.relays & 4, 100);
						gui->light(state.relays & 8, 100);
					} else {
						// visualize as two blinds
						if (state.relays & 1)
							state.blind1 = std::min(state.blind1 + ms, 100*65536);
						else if (state.relays & 2)
							state.blind1 = std::max(state.blind1 - ms, 0);
						gui->blind(state.blind1 >> 16);
						if (state.relays & 4)
							state.blind2 = std::min(state.blind2 + ms, 100*65536);
						else if (state.relays & 8)
							state.blind2 = std::max(state.blind2 - ms, 0);
						gui->blind(state.blind2 >> 16);
					}
					break;*/
				case EndpointType::TEMPERATURE_SENSOR:
					// temperature sensor
					{
						int temperature = gui->temperatureSensor(id++);
						if (temperature != -1) {
							uint8_t data[2];
							data[0] = temperature;
							data[1] = temperature >> 8;
							onBusReceived(state.endpointId, data, 2);
						}
					}
				}
			}
		}
		gui->newLine();
	}
}
