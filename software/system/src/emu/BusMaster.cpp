#include "../BusMaster.hpp"
#include <bus.hpp>
#include <crypt.hpp>
#include <util.hpp>
#include <emu/Gui.hpp>
#include <emu/Loop.hpp>
#include <iostream>
#include <fstream>
#include <chrono>


// emulator implementation of bus uses virtual devices on user interface
namespace BusMaster {

using PlugType = bus::PlugType;

constexpr int micLength = 4;


struct Endpoint {
	Array<PlugType const> plugs;
};

struct Device {
	struct Flash {
		uint8_t commissioning;
		uint8_t address;
		AesKey aesKey;
	};

	// device id
	uint32_t id;

	// two endpoint lists to test reconfiguration of device
	Array<Endpoint const> endpoints[2];

	Flash flash;
	uint8_t commissioning;
	uint32_t securityCounter;

	// attribute reading
	bool readAttribute;
	uint8_t endpointIndex;
	bus::Attribute attribute;

	// state
	int states[16];
};

constexpr auto SWITCH = PlugType::BINARY_SWITCH_WALL_OUT;
constexpr auto BUTTON = PlugType::BINARY_BUTTON_OUT;
constexpr auto ROCKER = PlugType::TERNARY_BUTTON_OUT;
constexpr auto LIGHT = PlugType::BINARY_POWER_LIGHT_CMD_IN;
constexpr auto BLIND = PlugType::TERNARY_OPENING_BLIND_IN;


const PlugType device1Plugs[] = {BUTTON, BUTTON, BLIND, BLIND};
const PlugType buttonPlugs[] = {BUTTON, BUTTON};
const PlugType rockerPlugs[] = {ROCKER, ROCKER};
const PlugType lightPlugs[] = {LIGHT, LIGHT};
const PlugType blindPlugs[] = {BLIND, BLIND};
const PlugType temperaturePlugs[] = {PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT};

// device 1
const Endpoint endpoint1a[] = {rockerPlugs, lightPlugs};
const Endpoint endpoint1b[] = {device1Plugs};//, blindPlugs};

// device 2
const Endpoint endpoint2[] = {buttonPlugs, rockerPlugs, blindPlugs, lightPlugs};

// device 3
const Endpoint endpoint3a[] = {temperaturePlugs};


Device devices[] = {
	{0x00000001, {endpoint1a, endpoint1b}},
	{0x00000002, {endpoint2, endpoint2}},
	{0x00000003, {endpoint3a, Array<Endpoint>()}},
};


std::fstream file;


std::chrono::steady_clock::time_point time;

Waitlist<ReceiveParameters> receiveWaitlist;
Waitlist<SendParameters> sendWaitlist;


void setHeader(bus::MessageWriter &w, Device &device) {
	w.setHeader();

	// encoded address
	w.address(device.flash.address);

	// security counter
	w.u32L(device.securityCounter);

	// set start of message
	w.setMessage();
}

void sendToMaster(bus::MessageWriter &w, Device &device) {
	// encrypt
	Nonce nonce(device.flash.address, device.securityCounter);
	w.encrypt(4, nonce, device.flash.aesKey);

	// increment security counter
	++device.securityCounter;

	// send to bus master (resume coroutine waiting to receive from device)
	int length = w.getLength();
	auto data = w.begin;
	BusMaster::receiveWaitlist.resumeFirst([length, data](ReceiveParameters &p) {
		int len = min(*p.receiveLength, length);
		array::copy(len, p.receiveData, data);
		*p.receiveLength = len;
		return true;
	});
}


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
		if (r.peekU8() == 0) {
			r.u8();
			if (r.u8() == 0) {
				// 0 0: master has sent a commissioning message

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
						setKey(device.flash.aesKey, r.data8<16>());

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
			int address = r.address();

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

					// get endpoint index
					uint8_t endpointIndex = r.u8();
					if (endpointIndex == 255) {
						// attribute
						device.endpointIndex = r.u8();
						device.attribute = r.e8<bus::Attribute>();
						device.readAttribute = r.atEnd();
					} else {
						// plug
						uint8_t plugIndex = r.u8();
						auto plugs = device.endpoints[device.flash.commissioning - 1][endpointIndex].plugs;
						if (plugIndex < plugs.count()) {
							auto plugType = plugs[plugIndex];
							int &state = device.states[endpointIndex * 4 + plugIndex];

							switch (plugType) {
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
							case PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_IN:
								state = r.u16L();
								break;
							default:;
							}
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

		// check if there is a pending read attribute
		if (device.readAttribute) {
			device.readAttribute = false;

			// send data message
			bus::MessageWriter w(sendData);
			setHeader(w, device);

			// "escaped" endpoint index
			w.u8(255);
			w.u8(device.endpointIndex);

			// attribute
			w.e8(device.attribute);

			switch (device.attribute) {
			case bus::Attribute::MODEL_IDENTIFIER:
				w.string("Bus");
				break;
			case bus::Attribute::PLUG_LIST:
				// list of plugs
				w.data16L(device.endpoints[device.commissioning - 1][device.endpointIndex].plugs);
				break;
			default:;
			}

			sendToMaster(w, device);
		}

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

			// protocol version
			w.u8(0);

			// number of endpoints
			w.u8(device.endpoints[device.commissioning - 1].count());

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
				auto plugs = endpoints[endpointIndex].plugs;
				for (int plugIndex = 0; plugIndex < plugs.count(); ++plugIndex) {
					auto endpointType = plugs[plugIndex];
					int &state = device.states[endpointIndex * 4 + plugIndex];

					// send data message
					bus::MessageWriter w(sendData);
					setHeader(w, device);

					// endpoint index
					w.u8(endpointIndex);

					// plug index
					w.u8(plugIndex);

					bool send = false;
					switch (endpointType) {
					case SWITCH: {
						// switch
						auto value = gui.onOff(id++);
						if (value) {
							state = *value;
							w.u8(state);
							send = true;
						}
						break;
					}
					case BUTTON: {
						// button
						auto value = gui.button(id++);
						if (value) {
							state = *value;
							w.u8(state);
							send = true;
						}
						break;
					}
					case ROCKER: {
						// rocker
						auto value = gui.rocker(id++);
						if (value) {
							state = *value;
							w.u8(state);
							send = true;
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
							blind = std::min(blind + us, 100 * 65536);
						else if (state & 2)
							blind = std::max(blind - us, 0);
						gui.blind(blind >> 16);
						state = (state & 3) | (blind << 2);
						break;
					}
					case PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT: {
						// temperature sensor
						auto temperature = gui.temperatureSensor(id++);
						if (temperature) {
							// convert temperature from Celsius to 1/20 Kelvin
							//state = int((*temperature + 273.15f) * 20.0f + 0.5f);
							w.f32L(*temperature + 273.15f);
							send = true;
							//w.u16L(state);
						}
						break;
					}

					default:
						break;
					}

					// check if we actually have to send the message
					if (send)
						sendToMaster(w, device);
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
