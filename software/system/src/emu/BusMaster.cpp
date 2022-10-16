#include "../BusMaster.hpp"
#include "StringBuffer.hpp"
#include "StringOperators.hpp"
#include <bus.hpp>
#include <crypt.hpp>
#include <util.hpp>
#include <emu/Gui.hpp>
#include <emu/Loop.hpp>
#include <chrono>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


// emulator implementation of bus uses virtual devices on user interface
namespace BusMaster {

using PlugType = bus::PlugType;

constexpr int micLength = 4;


struct Endpoint {
	Array<PlugType const> plugs;
};

struct Device {
	// persistent state
	struct PersistentState {
		int address;
		AesKey aesKey;
		uint32_t securityCounter;
		int index;
	};

	// device id
	uint32_t id;

	// two endpoint lists to test reconfiguration of device
	Array<Endpoint const> endpoints[2];

	// offset in file and persistent state that is stored in file
	int offset;
	PersistentState persistentState;

	// index to set when commissioned
	uint8_t nextIndex;

	// attribute reading
	bool readAttribute;
	uint8_t endpointIndex;
	bus::Attribute attribute;

	// state
	int states[16];
};

constexpr auto SWITCH = PlugType::BINARY_SWITCH_WALL_OUT;
constexpr auto BUTTON = PlugType::BINARY_BUTTON_OUT;
constexpr auto ROCKER = PlugType::TERNARY_ROCKER_OUT;
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


//std::fstream file;
int file;

inline int fsize(int fd) {
	struct stat stat;
	fstat(fd, &stat);
	return stat.st_size;
}


std::chrono::steady_clock::time_point time;

Waitlist<ReceiveParameters> receiveWaitlist;
Waitlist<SendParameters> sendWaitlist;


void setHeader(bus::MessageWriter &w, Device &device) {
	w.setHeader();

	// encoded address
	w.address(device.persistentState.address);

	// security counter
	w.u32L(device.persistentState.securityCounter);

	// set start of message
	w.setMessage();
}

void sendToMaster(bus::MessageWriter &w, Device &device) {
	// encrypt
	Nonce nonce(device.persistentState.address, device.persistentState.securityCounter);
	w.encrypt(4, nonce, device.persistentState.aesKey);

	// increment security counter
	++device.persistentState.securityCounter;
	pwrite(BusMaster::file, &device.persistentState.securityCounter, 4, device.offset + offsetOf(Device::PersistentState, securityCounter));

	// send to bus master (resume coroutine waiting to receive from device)
	int length = w.getLength();
	auto data = w.begin;
	BusMaster::receiveWaitlist.resumeFirst([length, data](ReceiveParameters &p) {
		int len = min(*p.length, length);
		array::copy(len, p.data, data);
		*p.length = len;
		return true;
	});
}


// event loop handler chain
Loop::Handler nextHandler;
void handle(Gui &gui) {
	// handle pending send operations
	BusMaster::sendWaitlist.resumeFirst([](SendParameters &p) {
		if (p.length < 2)
			return true;

		// copy data because it gets modified by decrypt()
		uint8_t data[64];
		array::copy(p.length, data, p.data);

		// read message
		bus::MessageReader r(p.length, data);
		r.setHeader();

		// get address
		if (r.peekU8() == 0) {
			r.u8();
			if (r.u8() == 0) {
				// 0 0: master has sent a commissioning message

				// get bus device id
				uint32_t deviceId = r.u32L();

				// check message integrity code (mic)
				r.setMessageFromEnd(micLength);
				Nonce nonce(deviceId, 0);
				if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
					return true;

				// get address
				auto address = r.u8();

				// search device with given bus device id
				for (Device &device: BusMaster::devices) {
					if (device.id == deviceId) {
						// set address
						device.persistentState.address = address;

						// set key
						setKey(device.persistentState.aesKey, r.data8<16>());

						// reset security counter
						device.persistentState.securityCounter = 0;

						// set index
						device.persistentState.index = device.nextIndex;

						// write commissioned device to file
						pwrite(BusMaster::file, &device.persistentState, sizeof(Device::PersistentState), device.offset);
					} else {
						if (device.persistentState.address == address) {
							// other device still has the address: decommission
							// todo: also do in switch
							device.persistentState.address = -1;
							pwrite(BusMaster::file, &device.persistentState, sizeof(Device::PersistentState), device.offset);
						}
					}
				}
			}
		} else {
			// master has sent a data message
			int address = r.address();

			// search device
			for (Device &device : BusMaster::devices) {
				if (device.persistentState.address == address) {
					// security counter
					uint32_t securityCounter = r.u32L();

					// todo: check security counter

					// decrypt
					r.setMessage();
					Nonce nonce(address, securityCounter);
					if (!r.decrypt(4, nonce, device.persistentState.aesKey))
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
						auto plugs = device.endpoints[device.persistentState.index][endpointIndex].plugs;
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

	// draw devices on gui
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
			case bus::Attribute::MODEL_IDENTIFIER: {
				StringBuffer<16> b;
				b.stream() << "Bus" << dec(device.id) << '.' << dec(device.endpointIndex);
				w.string(b.string());
				break;
			}
			case bus::Attribute::PLUG_LIST:
				// list of plugs
				w.data16L(device.endpoints[device.persistentState.index][device.endpointIndex].plugs);
				break;
			default:;
			}

			sendToMaster(w, device);
		}

		// commissioning can be triggered by a small commissioning button in the user interface
		auto value = gui.button(id++, 0.2f);
		if (value && *value == 1) {
			// commission device
			device.nextIndex = device.persistentState.address != -1 && device.persistentState.index == 0 ? 1 : 0;

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
			w.u8(device.endpoints[device.nextIndex].count());

			// add message integrity code (mic) using default key, message stays unencrypted
			w.setMessage();
			Nonce nonce(device.id, 0);
			w.encrypt(micLength, nonce, bus::defaultAesKey);

			// send to bus master (resume coroutine waiting to receive from device)
			int sendLength = w.getLength();
			BusMaster::receiveWaitlist.resumeFirst([sendLength, sendData](ReceiveParameters &p) {
				int length = min(*p.length, sendLength);
				array::copy(length, p.data, sendData);
				*p.length = length;
				return true;
			});
		}

		// add device endpoints to gui if device is commissioned
 		if (device.persistentState.address != -1) {
			auto endpoints = device.endpoints[device.persistentState.index];
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
	char const *filename = "bus.bin";
	BusMaster::file = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	int deviceCount = array::count(BusMaster::devices);
	int persistentStateSize = sizeof(Device::PersistentState);

	// check file size
	int fileSize = fsize(BusMaster::file);
	if (fileSize != deviceCount * persistentStateSize) {
		// size mismatch: initialize file
		ftruncate(BusMaster::file, deviceCount * persistentStateSize);
		for (int i = 0; i < deviceCount; ++i) {
			auto &device = BusMaster::devices[i];
			device.offset = i * persistentStateSize;
			device.persistentState.address = -1;
			pwrite(BusMaster::file, &device.persistentState, persistentStateSize, device.offset);
		}
	} else {
		// size ok: read devices
		for (int i = 0; i < deviceCount; ++i) {
			auto &device = BusMaster::devices[i];
			device.offset = i * persistentStateSize;
			pread(BusMaster::file, &device.persistentState, persistentStateSize, device.offset);
		}
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
