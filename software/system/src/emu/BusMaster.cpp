#include "../BusMaster.hpp"
#include <bus.hpp>
#include <crypt.hpp>
#include <util.hpp>
#include <emu/Gui.hpp>
#include <emu/Loop.hpp>
#include <iostream>
#include <fstream>


// emulator implementation of bus uses virtual devices on user interface
namespace BusMaster {

using EndpointType = bus::EndpointType;

constexpr int micLength = 4;


struct Device {
	struct Flash {
		uint8_t commissioning = 0;
		uint8_t address;
		AesKey aesKey;
	};

	uint32_t id;
	Array<EndpointType const> endpoints[2];
	Flash flash;
	uint8_t commissioning;
	uint32_t securityCounter;
	int states[16];
};

constexpr auto SWITCH = EndpointType::OFF_ON_OUT;
constexpr auto BUTTON = EndpointType::TRIGGER_OUT;
constexpr auto ROCKER = EndpointType::UP_DOWN_OUT;
constexpr auto LIGHT = EndpointType::OFF_ON_TOGGLE_IN;
constexpr auto BLIND = EndpointType::UP_DOWN_IN;

const EndpointType endpoints1a[] = {
	ROCKER, ROCKER, LIGHT, LIGHT, LIGHT,
};
const EndpointType endpoints1b[] = {
	ROCKER, ROCKER, ROCKER,BLIND, BLIND,
};
const EndpointType endpoints2[] = {
	ROCKER, BUTTON, BLIND, ROCKER, BUTTON, BLIND, LIGHT, LIGHT,
};
const EndpointType endpoints3a[] = {
	EndpointType::AIR_TEMPERATURE_OUT
};


Device devices[] = {
	{0x00000001, {endpoints1a, endpoints1b}},
	{0x00000002, {endpoints2, endpoints2}},
	{0x00000003, {endpoints3a, Array<EndpointType>()}},
};


std::fstream file;


std::chrono::steady_clock::time_point time;

Waitlist<ReceiveParameters> receiveWaitlist;
Waitlist<SendParameters> sendWaitlist;


// event loop handler chain
Loop::Handler nextHandler;
void handle(Gui &gui) {
	// handle pending send operations
	BusMaster::sendWaitlist.resumeFirst([](SendParameters &p) {
		if (p.sendLength < 2)
			return true;

		uint8_t data[64];
		array::copy(p.sendLength, data, p.sendData);

		// read message
		bus::MessageReader r(p.sendLength, data);
		r.setHeader();

		// get address
		auto a1 = r.arbiter();
		if (a1 == 0) {
			if (r.u8() == 0) {
				// master has sent a commissioning message

				// search device with given device id
				uint32_t deviceId = r.u32L();
				int offset = 0;
				for (Device &device: BusMaster::devices) {
					if (device.id == deviceId) {
						// check message integrity code (mic)
						r.setMessageFromEnd(micLength);
						Nonce nonce(deviceId, 0);
						if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
							break;

						// set address
						device.flash.address = r.u8();

						// set key
						setKey(device.flash.aesKey, r.data<16>());

						// reset security counter
						device.securityCounter = 0;

						// write to file
						device.flash.commissioning = device.commissioning;
						BusMaster::file.seekg(offset);
						BusMaster::file.write(reinterpret_cast<char *>(&device.flash), sizeof(Device::Flash));
						BusMaster::file.flush();

						break;
					}
					offset += sizeof(Device::Flash);
				}
			}
		} else {
			// master has sent a data message
			int address = a1 - 1 + (r.arbiter() << 3);

			// search device
			for (Device &device : BusMaster::devices) {
				if (device.flash.address == address) {
					// security counter
					uint32_t securityCounter = r.u32L();

					// todo: check security counter

					// decrypt
					r.setMessage();
					Nonce nonce(address, securityCounter);
					if (!r.decrypt(4, nonce, device.flash.aesKey))
						break;

					uint8_t endpointIndex = r.u8();
					auto endpoints = device.endpoints[device.flash.commissioning - 1];
					if (endpointIndex < endpoints.count()) {
						auto endpointType = endpoints[endpointIndex];
						int &state = device.states[endpointIndex];

						switch (endpointType) {
						case LIGHT: {
							// 0: off, 1: on, 2: toggle
							uint8_t s = r.u8();
							if (s < 2)
								state = s;
							else if (s == 2)
								state ^= 1;
							break;
						}
						case BLIND: {
							// 0: inactive, 1: up, 2: down
							uint8_t s = r.u8();
							state = (state & ~3) | s;
							break;
						}
						case EndpointType::AIR_TEMPERATURE_IN:
							state = r.u16L();
							break;
						default:;
						}
					}
					break;
				}
			}
		}
		return true;
	});

	// call next handler in chain
	BusMaster::nextHandler(gui);

	// time difference for blinds
	auto now = std::chrono::steady_clock::now();
	int us = int(std::chrono::duration_cast<std::chrono::microseconds>(now - BusMaster::time).count());
	BusMaster::time = now;

	// first id for gui
	uint32_t guiId = 0x81729a00;

	// iterate over devices
	for (Device &device : BusMaster::devices) {
		gui.newLine();
		auto id = guiId;
		guiId += 100;

		uint8_t sendData[64];

		// commissioning can be triggered by a small commissioning button in the user interface
		auto value = gui.button(id++, 0.2f);
		if (value && *value == 1) {
			// commission device
			device.commissioning = device.flash.commissioning == 1 ? 2 : 1;

			// send enumerate message
			bus::MessageWriter w(sendData);
			w.setHeader();

			// prefix with zero
			w.u8(0);

			// encoded device id
			w.id(device.id);

			// list of endpoints
			w.data(device.endpoints[device.commissioning - 1]);

			// add message integrity code (mic) using default key, message stays unencrypted
			w.setMessage();
			Nonce nonce(device.id, 0);
			w.encrypt(micLength, nonce, bus::defaultAesKey);

			// send to bus master (resume coroutine waiting to receive from device)
			int sendLength = w.getLength();
			BusMaster::receiveWaitlist.resumeFirst([sendLength, sendData](ReceiveParameters &p) {
				int length = min(*p.receiveLength, sendLength);
				array::copy(length, p.receiveData, sendData);
				*p.receiveLength = length;
				return true;
			});
		}

		// add device endpoints to gui if device is commissioned
 		if (device.flash.commissioning != 0) {
			auto endpoints = device.endpoints[device.flash.commissioning - 1];
			for (int endpointIndex = 0; endpointIndex < endpoints.count(); ++endpointIndex) {
				EndpointType endpointType = endpoints[endpointIndex];
				int &state = device.states[endpointIndex];

				// send data message
				bus::MessageWriter w(sendData);
				w.setHeader();

				// encoded address
				w.arbiter((device.flash.address & 7) + 1);
				w.arbiter(device.flash.address >> 3);

				// security counter
				w.u32L(device.securityCounter);

				// set start of message
				w.setMessage();

				// endpoint index
				w.u8(endpointIndex);

				bool send = false;
				switch (endpointType) {
					case SWITCH: {
						// switch
						auto value = gui.onOff(id++);
						if (value) {
							state = *value;
							send = true;
							w.u8(state);
						}
						break;
					}
					case BUTTON: {
						// button
						auto value = gui.button(id++);
						if (value) {
							state = *value;
							send = true;
							w.u8(state);
						}
						break;
					}
					case ROCKER: {
						// rocker
						auto value = gui.rocker(id++);
						if (value) {
							state = *value;
							send = true;
							w.u8(state);
						}
						break;
					}
					case LIGHT:
						// light
						gui.light(state & 1, 100);
						break;
					case BLIND: {
						// blind
						int blind = state >> 2;
						if (state & 1)
							blind = std::min(blind + us, 100*65536);
						else if (state & 2)
							blind = std::max(blind - us, 0);
						gui.blind(blind >> 16);
						state = (state & 3) | (blind << 2);
						break;
					}

					case EndpointType::AIR_TEMPERATURE_OUT: {
						// temperature sensor
						auto temperature = gui.temperatureSensor(id++);
						if (temperature) {
							// convert temperature from Celsius to 1/20 Kelvin
							state = int((*temperature + 273.15f) * 20.0f + 0.5f);
							send = true;
							w.u16L(state);
						}
						break;
					}

					default:
						break;
				}

				// check if we actually have to send the message
				if (send) {
					// encrypt
					Nonce nonce(device.flash.address, device.securityCounter);
					w.encrypt(4, nonce, device.flash.aesKey);

					// increment security counter
					++device.securityCounter;

					// send to bus master (resume coroutine waiting to receive from device)
					int sendLength = w.getLength();
					BusMaster::receiveWaitlist.resumeFirst([sendLength, sendData](ReceiveParameters &p) {
						int length = min(*p.receiveLength, sendLength);
						array::copy(length, p.receiveData, sendData);
						*p.receiveLength = length;
						return true;
					});
				}
			}
		}
	}
}

void init() {
	// check if already initialized
	if (BusMaster::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	BusMaster::nextHandler = Loop::addHandler(handle);

	// load permanent configuration of devices
	char const *fileName = "bus.bin";
	BusMaster::file.open(fileName, std::fstream::in | std::fstream::binary);
	for (Device &device : BusMaster::devices) {
		BusMaster::file.read(reinterpret_cast<char *>(&device.flash), sizeof(Device::Flash));
	}

	// read additional dummy byte to detect if the file is too large
	char dummy;
	BusMaster::file.read(&dummy, 1);
	BusMaster::file.clear();
	size_t size = BusMaster::file.tellg();
	BusMaster::file.close();

	// check if size is ok
	if (size != array::count(BusMaster::devices) * sizeof(Device::Flash)) {
		// no: create new file
		BusMaster::file.open(fileName, std::fstream::trunc | std::fstream::in | std::fstream::out | std::fstream::binary);
		for (Device &device : BusMaster::devices) {
			device.flash.commissioning = 0;
			BusMaster::file.write(reinterpret_cast<char *>(&device.flash), sizeof(Device::Flash));
		}
	} else {
		// yes: open file again in in/out mode
		BusMaster::file.open(fileName, std::fstream::in | std::fstream::out | std::fstream::binary);
	}
}

Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) {
	assert(BusMaster::nextHandler != nullptr);
	return {BusMaster::receiveWaitlist, &length, data};
}

Awaitable<SendParameters> send(int length, uint8_t const *data) {
	assert(BusMaster::nextHandler != nullptr);
	return {BusMaster::sendWaitlist, length, data};
}

} // namespace BusMaster
