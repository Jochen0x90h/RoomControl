#include <bus.hpp>
#include <BusMaster.hpp>
#include <crypt.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <util.hpp>
#include <emu/Gui.hpp>
#include <emu/Loop.hpp>
#include <iostream>
#include <fstream>


// emulator implementation of bus uses virtual devices on user interface
namespace BusMaster {

using EndpointType = bus::EndpointType;

AesKey const busDefaultAesKey = {{0x1337c6b3, 0xf16c7cb6, 0x2dec182d, 0x3078d618, 0xaec16bb7, 0x5fad1701, 0x72410f2c, 0x4239d934, 0xbef4739b, 0xe159649a, 0x93186bb6, 0xd121b282, 0x47c360a5, 0xa69a043f, 0x35826f89, 0xe4a3dd0b, 0x45024bcc, 0xe3984ff3, 0xd61a207a, 0x32b9fd71, 0x0356e8ef, 0xe0cea71c, 0x36d48766, 0x046d7a17, 0x1f8c181d, 0xff42bf01, 0xc9963867, 0xcdfb4270, 0x50a049a0, 0xafe2f6a1, 0x6674cec6, 0xab8f8cb6, 0xa3c407c2, 0x0c26f163, 0x6a523fa5, 0xc1ddb313, 0x79a97aba, 0x758f8bd9, 0x1fddb47c, 0xde00076f, 0x2c6cd2a7, 0x59e3597e, 0x463eed02, 0x983eea6d}};

constexpr int micLength = 4;


class MessageReader : public DecryptReader {
public:
	MessageReader(int length, uint8_t *data) : DecryptReader(length, data) {}

	/**
	 * Read a value from 0 to 8 from bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 */
	uint8_t arbiter() {
		uint8_t value = u8();

		uint8_t count = 0;
		while (value > 0) {
			++count;
			value <<= 1;
		}
		return count;
	}
};

class MessageWriter : public EncryptWriter {
public:
	template <int N>
	MessageWriter(uint8_t (&message)[N]) : EncryptWriter(message), begin(message)
#ifdef EMU
		, end(message + N)
#endif
	{}

	MessageWriter(int length, uint8_t *message) : EncryptWriter(message), begin(message)
#ifdef EMU
		, end(message + length)
#endif
	{}

	/**
	 * Write a value from 0 to 8 for bus arbitration, i.e. multiple devices can write at the same time and the
	 * lowest value survives
	 */
	void arbiter(uint8_t value) {
		u8(~(0xff >> value));
	}

	int getLength() {
		int length = this->current - this->begin;
#ifdef EMU
		assert(this->current <= this->end);
#endif
		return length;
	}

	uint8_t *begin;
#ifdef EMU
	uint8_t *end;
#endif
};


struct Device {
	struct Flash {
		bool commissioned = false;
		uint8_t address;
		AesKey aesKey;
	};

	uint32_t id;
	Array<EndpointType const> endpoints;
	Flash flash;
	uint16_t securityCounter;
	int states[16];
	int requestFlags = 0;
};

constexpr auto SWITCH = EndpointType::ON_OFF_IN;
constexpr auto BUTTON = EndpointType::TRIGGER_IN;
constexpr auto ROCKER = EndpointType::UP_DOWN_IN;
constexpr auto LIGHT = EndpointType::ON_OFF_OUT;
constexpr auto BLIND = EndpointType::UP_DOWN_OUT;

const EndpointType endpoints1[] = {
	ROCKER, ROCKER, LIGHT, LIGHT, LIGHT,
};
const EndpointType endpoints2[] = {
	ROCKER, BUTTON, BLIND, ROCKER, BUTTON, BLIND, LIGHT, LIGHT,
};
const EndpointType endpoints3[] = {
	EndpointType::TEMPERATURE_IN
};

Device devices[] = {
	{0x00000001, endpoints1},
	{0x00000002, endpoints2},
	{0x00000003, endpoints3},
};


std::fstream file;


std::chrono::steady_clock::time_point time;

Waitlist<Parameters> transferWaitlist;
Waitlist<> requestWaitlist;


// event loop handler chain
Loop::Handler nextHandler;
void handle(Gui &gui) {
	// handle pending bus transfers
	BusMaster::transferWaitlist.resumeFirst([](Parameters &p) {
		// loop back write data to emulate physical bus
		int readLength = p.readLength;
		p.readLength = min(readLength, p.writeLength);
		array::copy(p.readLength, p.readData, p.writeData);

		if (p.writeLength == 0) {
			// read from device: search for a device that has requested to be read
			for (Device &device : BusMaster::devices) {
				if (device.flash.commissioned && device.requestFlags != 0) {
					for (int endpointIndex = 0; endpointIndex < device.endpoints.count(); ++endpointIndex) {
						EndpointType endpointType = device.endpoints[endpointIndex];
						int flag = 1 << endpointIndex;
						if ((device.requestFlags & flag) != 0) {
							device.requestFlags &= ~flag;

							// write into read data of transfer()
							MessageWriter w(readLength, p.readData);

							// set start of header
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

							int state = device.states[endpointIndex];
							switch (endpointType) {
							case EndpointType::ON_OFF_IN:
							case EndpointType::TRIGGER_IN:
							case EndpointType::UP_DOWN_IN:
								w.u8(state);
								break;
							case EndpointType::TEMPERATURE_IN:
								w.u16L(state);
								break;
							default:
								break;
							}

							// encrypt
							Nonce nonce(device.flash.address, device.securityCounter);
							w.encrypt(4, nonce, device.flash.aesKey);

							// increment security counter
							++device.securityCounter;

							p.readLength = w.getLength();

							return true;
						}
					}
				}
			}
		} else if (p.writeData[0] > 0) {
			// write to device: get address and search device
			uint8_t data[64];
			array::copy(Array<uint8_t>(data), p.writeData);
			MessageReader r(p.writeLength, data);
			r.setHeader();
			uint8_t address = (r.arbiter() - 1) | (r.arbiter() << 3);
			for (Device &device : BusMaster::devices) {
				if (device.flash.address == address) {
					// security counter
					uint32_t securityCounter = r.u32L();

					// todo: check security counter

					r.setMessage();

					// decrypt
					Nonce nonce(address, securityCounter);
					if (!r.decrypt(4, nonce, device.flash.aesKey))
						break;

					uint8_t endpointIndex = r.u8();
					if (endpointIndex < device.endpoints.count()) {
						EndpointType endpointType = device.endpoints[endpointIndex];
						int &state = device.states[endpointIndex];

						switch (endpointType) {
						case EndpointType::ON_OFF:
							{
								uint8_t s = r.u8();
								if (s < 2)
									state = s;
								else
									state ^= 1;
							}
							break;
						case EndpointType::TRIGGER:
						case EndpointType::UP_DOWN:
							state = (state & ~3) | r.u8();
							break;
						case EndpointType::TEMPERATURE:
							state = r.u16L();
							break;
						default:
							break;
						}
					}
					break;
 				}
			}
		} else {
			// command (leading zero byte)
			if (p.writeLength == 1) {
				// enumerate: search a device that has not been commissioned
				for (Device &device : BusMaster::devices) {
					if (!device.flash.commissioned) {
						// write into read data of transfer()
						MessageWriter w(readLength, p.readData);
						w.setHeader();

						// skip zero
						w.skip(1);

						// encode device id
						uint32_t id = device.id;
						for (int i = 0; i < 11; ++i) {
							w.arbiter((id & 3) + 1);
							id >>= 3;
						}

						// endpoints
						w.data(device.endpoints);

						// start of message (is empty, everything is in the header)
						w.setMessage();

						// encrypt
						Nonce nonce(0, 0);
						w.encrypt(micLength, nonce, busDefaultAesKey);

						p.readLength = w.getLength();

						break;
					}
				}
			} else if (p.writeData[1] == 0) {
				// commission
				uint8_t data[64];
				array::copy(p.writeLength, data, p.writeData);
				MessageReader r(p.writeLength, data);
				r.setHeader();

				// skip command prefix (0) and arbitration byte (0)
				r.skip(2);

				// search device with given device id
				uint32_t deviceId = r.u32L();
				int offset = 0;
				for (Device &device : BusMaster::devices) {
					if (device.id == deviceId) {
						//r.setMessage(data + p.writeLength - 4);
						r.setMessageFromEnd(micLength);

						// decrypt
						Nonce nonce(0, 0);
						if (!r.decrypt(micLength, nonce, busDefaultAesKey))
							break;

						// set address
						device.flash.address = r.u8();

						// set key
						setKey(device.flash.aesKey, r.data<16>());

						// write to file
						device.flash.commissioned = true;
						BusMaster::file.seekg(offset);
						BusMaster::file.write(reinterpret_cast<char *>(&device.flash), sizeof(Device::Flash));
						BusMaster::file.flush();

						break;
					}
					offset += sizeof(Device::Flash);
				}
			}
		}
		return true;
	});

	// call next handler in chain
	BusMaster::nextHandler(gui);

	// time difference
	auto now = std::chrono::steady_clock::now();
	int us = std::chrono::duration_cast<std::chrono::microseconds>(now - BusMaster::time).count();
	BusMaster::time = now;

	// id for gui
	int id = 0x81729a00;

	// iterate over devices
	for (Device &device : BusMaster::devices) {
		gui.newLine();

		// check if device is commissioned
		if (!device.flash.commissioned)
			continue;

		// iterate over endpoints
		for (int endpointIndex = 0; endpointIndex < device.endpoints.count(); ++endpointIndex) {
			EndpointType endpointType = device.endpoints[endpointIndex];
			int &state = device.states[endpointIndex];

			switch (endpointType) {
			case SWITCH:
				// switch
				{
					int value = gui.switcher(id++);
					if (value != -1) {
						state = value;

						// mark dirty and resume coroutine that awaits a request so that it can receive data
						device.requestFlags |= 1 << endpointIndex;
						BusMaster::requestWaitlist.resumeFirst();
					}
				}
				break;
			case LIGHT:
				// light
				gui.light(state & 1, 100);
				break;
			case BUTTON:
				// button
				{
					int value = gui.button(id++);
					if (value != -1) {
						state = value;

						// mark dirty and resume coroutine that awaits a request so that it can receive data
						device.requestFlags |= 1 << endpointIndex;
						BusMaster::requestWaitlist.resumeFirst();
					}
				}
				break;
			case ROCKER:
				// rocker
				{
					int value = gui.rocker(id++);
					if (value != -1) {
						state = value;

						// mark dirty and resume coroutine that awaits a request so that it can receive data
						device.requestFlags |= 1 << endpointIndex;
						BusMaster::requestWaitlist.resumeFirst();
					}
				}
				break;
			case BLIND:
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

			case EndpointType::TEMPERATURE_IN:
				// temperature sensor
				{
					auto temperature = gui.temperatureSensor(id++);
					if (temperature) {
						// convert temperature from Celsius to 1/20 Kelvin
						state = int((*temperature + 273.15f) * 20.0f);

						// mark dirty and resume coroutine that awaits a request so that it can receive data
						device.requestFlags |= 1 << endpointIndex;
						BusMaster::requestWaitlist.resumeFirst();
					}
				}
				break;

			default:
				break;
			}
		}
		id = (id + 256) & 0xffffff00;
	}
}

void init() {
	// add to event loop handler chain
	BusMaster::nextHandler = Loop::addHandler(handle);

	// load permanent configuration of devices
	char const *fileName = "bus.bin";
	BusMaster::file.open(fileName, std::fstream::in | std::fstream::binary);
	for (Device &device : BusMaster::devices) {
		BusMaster::file.read(reinterpret_cast<char *>(&device.flash), sizeof(Device::Flash));
	}
	char dummy;
	BusMaster::file.read(&dummy, 1);
	BusMaster::file.clear();
	size_t size = BusMaster::file.tellg();
	BusMaster::file.close();

	// check if size is ok
	if (size != 3 * sizeof(Device::Flash)) {
		// no: create new file
		BusMaster::file.open(fileName, std::fstream::trunc | std::fstream::in | std::fstream::out | std::fstream::binary);
		for (Device &device : BusMaster::devices)
			device.flash.commissioned = false;
	} else {
		// yes: open file again
		BusMaster::file.open(fileName, std::fstream::in | std::fstream::out | std::fstream::binary);
	}
}

Awaitable<Parameters> transfer(int writeLength, uint8_t const *writeData, int &readLength, uint8_t *readData) {
	return {BusMaster::transferWaitlist, writeLength, writeData, readLength, readData};
}

Awaitable<> request() {
	return {BusMaster::requestWaitlist};
}

} // namespace BusMaster
