#include <bus.hpp>
#include <BusInterface.hpp>
#include <Gui.hpp>
#include <util.hpp>
#include <emu/global.hpp>
#include <iostream>


namespace bus {

// list of emulated devices
constexpr uint32_t deviceIds[] = {
	0x00000001,
	0x00000002,
	0x00000003
};

constexpr EndpointType endpointTypes[] = {
	EndpointType::ROCKER, EndpointType::ROCKER, EndpointType::LIGHT, EndpointType::LIGHT, EndpointType::LIGHT,
	EndpointType::ROCKER, EndpointType::BUTTON, EndpointType::BLIND, EndpointType::ROCKER, EndpointType::BUTTON, EndpointType::BLIND, EndpointType::LIGHT, EndpointType::LIGHT,
	EndpointType::TEMPERATURE_SENSOR};
constexpr int endpointStarts[] = {0, 5, 5+8, 5+8+1};

uint8_t endpointIds[array::size(endpointTypes) + array::size(deviceIds)];

int states[array::size(endpointTypes)];

std::chrono::steady_clock::time_point time;

std::function<void (uint8_t)> onRequest;


void doGui(Gui &gui, int &id) {

	// time difference
	auto now = std::chrono::steady_clock::now();
	int us = std::chrono::duration_cast<std::chrono::microseconds>(now - bus::time).count();
	bus::time = now;
	
	for (int deviceIndex = 0; deviceIndex < array::size(bus::deviceIds); ++deviceIndex) {
		for (int i = bus::endpointStarts[deviceIndex]; i < bus::endpointStarts[deviceIndex + 1]; ++i) {
			uint8_t endpointId = bus::endpointIds[i];
			
			// check if someone subscribed to this topic
			if (endpointId != 0) {
				int &state = bus::states[i];
				switch (bus::endpointTypes[i]) {
				case EndpointType::BINARY_SENSOR:
					break;
				case EndpointType::BUTTON:
					// button
					{
						int value = gui.button(id++);
						if (value != -1) {
							state = value;
							asio::post(global::context, [endpointId]() {bus::onRequest(endpointId);});
						}
					}
					break;
				case EndpointType::SWITCH:
					// switch
					{
						int value = gui.switcher(id++);
						if (value != -1) {
							state = value;
							asio::post(global::context, [endpointId]() {bus::onRequest(endpointId);});
						}
					}
					break;
				case EndpointType::ROCKER:
					// rocker
					{
						int value = gui.rocker(id++);
						if (value != -1) {
							state = value;
							asio::post(global::context, [endpointId]() {bus::onRequest(endpointId);});
						}
					}
					break;
				case EndpointType::RELAY:
					// relay
					break;
				case EndpointType::LIGHT:
					// light
					gui.light(state & 1, 100);
					break;
				case EndpointType::BLIND:
					// blind
					{
						int blind = state >> 2;
						if (state & 1)
							blind = std::min(blind + us, 100*65536);
						else if (state & 2)
							blind = std::max(blind - us, 0);
						gui.blind(blind >> 16);
						state = (state & 3) | (blind << 2);
					}
					break;

				case EndpointType::TEMPERATURE_SENSOR:
					// temperature sensor
					{
						int temperature = gui.temperatureSensor(id++);
						if (temperature != -1) {
							state = temperature;
							//onBusReceived(state.endpointId, data, 2);
							asio::post(global::context, [endpointId]() {bus::onRequest(endpointId);});
						}
					}
				}
			}
		}
		gui.newLine();
	}
}


void init() {
}

void handle() {
}

void transfer(uint8_t const *txData, int txLength, uint8_t *rxData, int rxLength, std::function<void (int)> onRx) {
	// always receive what we send
	assert(rxLength >= txLength);
	for (int i = 0; i < txLength; ++i)
		rxData[i] = txData[i];
	int rxIndex = txLength;
	
	// check checksum if data gets written
	if (txLength > 1) {
		assert(txData[txLength - 1] == BusInterface::calcChecksum(txData, txLength - 1));
	}
	
	uint8_t endpointId = txData[0];
	if (endpointId == 0) {
		// get next device id and descriptor (next device that has not an endpoint yet)
		for (int i = 0; i < array::size(bus::deviceIds); ++i) {
			if (bus::endpointIds[array::size(endpointTypes) + i] == 0) {
				uint32_t deviceId = bus::deviceIds[i];
				rxData[rxIndex++] = deviceId;
				rxData[rxIndex++] = deviceId >> 8;
				rxData[rxIndex++] = deviceId >> 16;
				rxData[rxIndex++] = deviceId >> 24;
				
				int start = bus::endpointStarts[i];
				int end = bus::endpointStarts[i + 1];
				for (int i = start; i < end; ++i) {
					rxData[rxIndex++] = uint8_t(bus::endpointTypes[i]);
				}
				break;
			}
		}
	} else if (endpointId == 1) {
		// set device endpoint (each device has an endpoint which is used for subscribe/unsubscribe)
		if (txLength >= 7) {
			uint32_t deviceId = (txData[4] << 24) | (txData[3] << 16) | (txData[2] << 8) | txData[1];
			uint8_t deviceEndpointId = txData[5];
			
			for (int i = 0; i < array::size(bus::deviceIds); ++i) {
				if (bus::deviceIds[i] == deviceId) {
					bus::endpointIds[array::size(endpointTypes) + i] = deviceEndpointId;
					break;
				}
			}
		}
	} else {
		for (int i = 0; i < array::size(bus::endpointIds); ++i) {
			if (bus::endpointIds[i] == endpointId) {
				int &state = states[i];
				if (i < array::size(endpointTypes)) {
					// read / write data
					EndpointType type = endpointTypes[i];
					
					switch (type) {
					case EndpointType::BINARY_SENSOR:
						rxData[rxIndex++] = state;
						break;
					case EndpointType::BUTTON:
						rxData[rxIndex++] = state;
						break;
					case EndpointType::SWITCH:
						rxData[rxIndex++] = state;
						break;
					case EndpointType::ROCKER:
						rxData[rxIndex++] = state;
						break;
					
					case EndpointType::RELAY:
						if (txLength >= 2)
							state = txData[1];
						break;
					case EndpointType::LIGHT:
						if (txLength >= 2)
							state = txData[1];
						break;
					case EndpointType::BLIND:
						if (txLength >= 2)
							state = (state & ~3) | (txData[1] & 3);
						break;
					
					case EndpointType::TEMPERATURE_SENSOR:
						rxData[rxIndex++] = state;
						rxData[rxIndex++] = state >> 8;
						break;
						
					default:
						break;
					}
				} else {
					// subscribe / unsubscribe topic
					int deviceIndex = i - array::size(endpointTypes);
					uint8_t endpointIndex = txData[1];
					uint8_t endpointId = txData[2]; // 0 to unsubscribe
					int topicIndex = endpointStarts[deviceIndex] + endpointIndex;
					bus::endpointIds[topicIndex] = endpointId;
				}
				break;
			}
		}
	}

	// append checksum if we sent data
	if (rxIndex > txLength) {
		rxData[rxIndex] = BusInterface::calcChecksum(rxData, rxIndex);
		++rxIndex;
	}

	asio::post(global::context, [onRx, rxIndex]() {onRx(rxIndex);});
}

void setRequestHandler(std::function<void (uint8_t)> onRequest) {
	bus::onRequest = onRequest;
}

} // namespace bus
