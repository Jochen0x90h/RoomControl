#include "BusInterface.hpp"
#include <BusMaster.hpp>
#include <Terminal.hpp>
#include <Timer.hpp>
#include <Nonce.hpp>
#include <StringOperators.hpp>
#include <util.hpp>


constexpr int maxMessageLength = 64;
constexpr int micLength = 4;


// BusInterface

BusInterface::BusInterface(PersistentStateManager &stateManager)
	: securityCounter(stateManager) {
	// set backpointers
	for (auto &device: this->devices)
		device.interface = this;
}

BusInterface::~BusInterface() {
}

Coroutine BusInterface::start(DataBuffer<16> const &key, AesKey const &aesKey) {
	this->key = &key;
	this->aesKey = &aesKey;

	// restore security counter
	co_await this->securityCounter.restore();

	// start coroutines
	receive();
	publish();
}

void BusInterface::setCommissioning(bool enabled) {
	this->commissioning = enabled;
}

int BusInterface::getDeviceCount() {
	return this->devices.count();
}

Interface::Device &BusInterface::getDeviceByIndex(int index) {
	assert(index >= 0 && index < this->devices.count());
	return this->devices[index];
}

Interface::Device *BusInterface::getDeviceById(uint8_t id) {
	for (auto &device: this->devices) {
		if (device->interfaceId == id)
			return &device;
	}
	return nullptr;
}

static bool readMessage(MessageType dstType, void *dstMessage, MessageType srcType, MessageReader r,
	ConvertOptions const &convertOptions)
{
	Message &dst = *reinterpret_cast<Message *>(dstMessage);

	switch (srcType) {
		case MessageType::OFF_ON_IN:
		case MessageType::OFF_ON_TOGGLE_IN:
		case MessageType::TRIGGER_IN:
		case MessageType::UP_DOWN_IN:
		case MessageType::OPEN_CLOSE_IN:
			return convertCommandIn(dstType, dst, r.u8(), convertOptions);
		case MessageType::LEVEL_IN: {
			// convert level from 0 - 255 to 0.0 - 1.0
			float level = float(r.u8()) / 255.0f;
			return convertFloatValueIn(dstType, dst, srcType, level, convertOptions);
		}
		case MessageType::AIR_TEMPERATURE_IN: {
			// convert temperature from 1/20 Kelvin to Kelvin
			float temperature = float(r.u16L()) * 0.05f;
			return convertFloatValueIn(dstType, dst, srcType, temperature, convertOptions);
		}
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

Coroutine BusInterface::receive() {
	uint8_t receiveMessage[maxMessageLength];

	while (true) {
		int receiveLength = maxMessageLength;
		co_await BusMaster::receive(receiveLength, receiveMessage);

		for (int i = 0; i < receiveLength; ++i)
			Terminal::out << hex(receiveMessage[i]) << ' ';
		Terminal::out << '\n';


		bus::MessageReader r(receiveLength, receiveMessage);

		// set start of header
		r.setHeader();

		// get address
		uint8_t a1 = r.arbiter();
		if (a1 == 0) {
			// enumerate message
			if (!this->commissioning)
				continue;

			// get device id
			uint32_t deviceId = r.id();

			// check message integrity code (mic)
			r.setMessageFromEnd(micLength);
			Nonce nonce(deviceId, 0);
			if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
				continue;

			// create a new device
			DeviceFlash flash;
			flash.deviceId = deviceId;
			flash.interfaceId = allocateInterfaceId(this->devices);

			int index = this->devices.count();
			for (int i = 0; i < this->devices.count(); ++i) {
				auto &device = this->devices[i];
				if (device->deviceId == deviceId) {
					index = i;
					flash.interfaceId = device->interfaceId;
					break;
				}
			}
			flash.address = flash.interfaceId - 1;


			// get endpoints
			flash.endpointCount = r.getRemaining();
			array::copy(flash.endpointCount, flash.endpoints, r.current);

			// commission the device
			constexpr int commissionLength =
				1 + 1 + 4 + 1 + 16 + micLength; // command prefix, arbitration, device id, address, key, mic
			uint8_t sendMessage[commissionLength];
/*
			// check if device already exists and set flag for each used address to find a free address
			array::fill(9, sendMessage, 0);
			int plugIndex = this->devices.count();
			for (int i = 0; i < this->devices.count(); ++i) {
				if (this->devices[i]->deviceId == deviceId)
					plugIndex = i;
				else
					sendMessage[i >> 3] |= 1 << (i & 7);
			}

			// find free address
			int j;
			for (j = 0; j < 9; ++j) {
				uint8_t f = sendMessage[j];
				if (f != 0xff) {
					for (int i = 0; i < 8; ++i) {
						if ((f & (1 << (i & 7))) == 0) {
							flash.address = j * 8 + i;
							break;
						}
					}
					break;
				}
			}

			// continue if no free address found
			if (j == 9)
				continue;
*/
			if (flash.address >= 8 * 9)
				continue;
			Terminal::out << "bus device " << hex(deviceId) << ": assigned address " << dec(flash.address) << '\n';

			// send commissioning message
			{
				bus::MessageWriter w(sendMessage);

				// set start of header
				w.setHeader();

				// command prefix
				w.u8(0);

				// arbitration byte that always "wins" against enumeration messages sent by devices
				w.u8(0);

				// device id
				w.u32L(deviceId);

				// address that gets assigned to the device
				w.u8(flash.address);

				// key
				w.data(*this->key);

				// only add message integrity code (mic) using default key, message stays unencrypted
				w.setMessage();
				Nonce nonce(deviceId, 0);
				w.encrypt(micLength, nonce, bus::defaultAesKey);

				int sendLength = w.getLength();
				co_await BusMaster::send(sendLength, sendMessage);
			}

			// write the configured device to flash
			if (index < this->devices.count()) {
				// only update flash, keep subscribers
				this->devices.write(index, flash);
			} else {
				// create new device
				BusDevice *device = new BusDevice(flash);
				device->interface = this;
				this->devices.write(index, device);
			}
		} else {
			// data message
			uint8_t address = a1 - 1 + r.arbiter() * 8;

			// get security counter
			uint32_t securityCounter = r.u32L();

			// set start of encrypted message
			r.setMessage();

			// decrypt
			Nonce nonce(address, securityCounter);
			if (!r.decrypt(micLength, nonce, *this->aesKey)) {
				Terminal::out << "bus: decrypt failed!\n";
				continue;
			}

			for (auto &device: this->devices) {
				auto const &flash = *device;
				if (flash.address == address) {
					uint8_t endpointIndex = r.u8();

					// publish to subscribers
					for (auto &subscriber: device.subscribers) {
						// check if this is the right endpoint
						if (subscriber.index == endpointIndex) {
							MessageType srcType = flash.endpoints[endpointIndex];
							subscriber.barrier->resumeFirst([&subscriber, &r, srcType](Subscriber::Parameters &p) {
								p.source = subscriber.source;

								// read message (note r is passed by value for multiple subscribers)
								return readMessage(subscriber.messageType, p.message, srcType, r,
									subscriber.convertOptions);
							});
						}
					}
					break;
				}
			}
		}
	}
}

static bool writeMessage(MessageWriter &w, MessageType dstType, Publisher &publisher) {
	auto srcType = publisher.messageType;
	Message const &src = *reinterpret_cast<Message const *>(publisher.message);

	switch (dstType) {
		case MessageType::OFF_ON_OUT:
		case MessageType::OFF_ON_TOGGLE_OUT:
		case MessageType::TRIGGER_OUT:
		case MessageType::UP_DOWN_OUT:
		case MessageType::OPEN_CLOSE_OUT:
			return convertCommandOut(w.u8(), srcType, src, publisher.convertOptions);
		case MessageType::LEVEL_OUT: {
			float level;
			if (!convertFloatValueOut(dstType, level, srcType, src, publisher.convertOptions))
				return false; // conversion failed
			w.u8(clamp(int(level * 255.0f + 0.5f), 0, 255));
			break;
		}
		case MessageType::SET_LEVEL_OUT:
		case MessageType::MOVE_TO_LEVEL_OUT: {
			Message level;
			if (!convertSetFloatValueOut(dstType, level, srcType, src, publisher.convertOptions))
				return false; // conversion failed

			w.e8<bus::LevelControlCommand>(bus::LevelControlCommand(level.command));
			w.u8(clamp(int(level.value.f * 255.0f + 0.5f), 0, 255));

			if (dstType == MessageType::MOVE_TO_LEVEL_OUT) {
				// duration in 1/10s
				w.u16L(level.transition);
			}
			break;
		}
/*
			switch (srcType) {
				case MessageType::LEVEL_OUT:
				case MessageType::MOVE_TO_LEVEL_OUT: {
					bus::LevelControlCommand command = !src.level.getFlag() ? bus::LevelControlCommand::SET
						: (src.level.isPositive() ? bus::LevelControlCommand::INCREASE : bus::LevelControlCommand::DECREASE);
					uint8_t value = clamp(abs(int(src.level * 256.0f)), 0, 255);

					if (dstType == MessageType::LEVEL_OUT || srcType == MessageType::LEVEL_OUT) {
						w.e8<bus::LevelControlCommand>(command);
						w.u8(value);
					} else {
						if (!src.moveToLevel.move.getFlag()) {
							w.e8<bus::LevelControlCommand>(command | bus::LevelControlCommand::DURATION);
							w.u8(value);
							w.u16L(clamp(int(src.moveToLevel.move * 10.0f), 0, 65535)); // 1/10 s
						} else {
							w.e8<bus::LevelControlCommand>(command | bus::LevelControlCommand::SPEED);
							w.u8(value);
							w.u16L(clamp(int(src.moveToLevel.move * 5000.0f), 0, 65535)); // 1 / 5000s
						}
					}
					break;
				}
				default:
					// conversion failed
					return false;
			}
			break;
*/
		case MessageType::AIR_TEMPERATURE_OUT: {
			float temperature;
			if (!convertFloatValueOut(dstType, temperature, srcType, src, publisher.convertOptions))
				return false; // conversion failed
			w.u16L(clamp(int(temperature * 20.0f + 0.5f), 0, 65535));
			break;
		}
		case MessageType::SET_AIR_TEMPERATURE_OUT: {
			Message temperature;
			if (!convertSetFloatValueOut(dstType, temperature, srcType, src, publisher.convertOptions))
				return false; // conversion failed

			w.e8<bus::LevelControlCommand>(bus::LevelControlCommand(temperature.command));
			w.u16L(clamp(int(temperature.value.f * 20.0f + 0.5f), 0, 65535));
			break;
		}

		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

Coroutine BusInterface::publish() {
	uint8_t outMessage[maxMessageLength];
	uint8_t inMessage[maxMessageLength];
	while (true) {
		// wait until something was published
		co_await this->publishEvent.wait();

		// clear immediately as we have only one instance of this coroutine
		this->publishEvent.clear();

		// iterate over devices
		for (auto &device: this->devices) {
			// iterate over publishers
			for (auto &publisher: device.publishers) {
				// check if publisher wants to publish
				if (publisher.dirty) {
					publisher.dirty = false;

					// get endpoint info
					uint8_t endpointIndex = publisher.index;
					MessageType messageType = device->endpoints[endpointIndex];

					if ((messageType & MessageType::DIRECTION_MASK) == MessageType::OUT) {
						// send data message to node
						int sendLength;
						{
							DeviceFlash const &flash = *device;

							bus::MessageWriter w(outMessage);

							// set start of header
							w.setHeader();

							// encoded address
							w.arbiter((flash.address & 7) + 1);
							w.arbiter(flash.address >> 3);

							// security counter
							w.u32L(this->securityCounter);

							// set start of message
							w.setMessage();

							// endpoint index
							w.u8(endpointIndex);

							// message data
							if (!writeMessage(w, messageType, publisher))
								break;

							// encrypt
							Nonce nonce(flash.address, this->securityCounter);
							w.encrypt(micLength, nonce, *this->aesKey);

							//bool ok = decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, this->configuration->networkAesKey);

							// increment security counter
							++this->securityCounter;

							sendLength = w.getLength();
						}
						co_await BusMaster::send(sendLength, outMessage);
					}
/*
					// forward to subscribers
					for (auto &subscriber: device.subscribers) {
						if (subscriber.index == publisher.index) {
							subscriber.barrier->resumeAll([&subscriber, &publisher](Subscriber::Parameters &p) {
								p.subscriptionIndex = subscriber.subscriptionIndex;

								// convert to target unit and type and resume coroutine if conversion was successful
								return convert(subscriber.messageType, p.message,
									publisher.messageType, publisher.message);
							});
						}
					}*/
				}
			}
		}
	}
}


// BusInterface::DeviceFlash

int BusInterface::DeviceFlash::size() const {
	return getOffset(DeviceFlash, endpoints[this->endpointCount]);
}

BusInterface::BusDevice *BusInterface::DeviceFlash::allocate() const {
	return new BusDevice(*this);
}


// BusInterface::BusDevice

BusInterface::BusDevice::~BusDevice() {
	for (auto &publisher : this->publishers)
		publisher.event = nullptr;
}

uint8_t BusInterface::BusDevice::getId() const {
	auto &flash = **this;
	return flash.interfaceId;
}

String BusInterface::BusDevice::getName() const {
	return "x";
}

void BusInterface::BusDevice::setName(String name) {

}

Array<MessageType const> BusInterface::BusDevice::getEndpoints() const {
	auto &flash = **this;
	return {flash.endpointCount, flash.endpoints};
}

void BusInterface::BusDevice::addPublisher(uint8_t endpointIndex, Publisher &publisher) {
	publisher.remove();
	publisher.index = endpointIndex;
	publisher.event = &this->interface->publishEvent;
	this->publishers.add(publisher);
}

void BusInterface::BusDevice::addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.index = endpointIndex;
	this->subscribers.add(subscriber);
}
