#include "BusInterface.hpp"
#include <BusMaster.hpp>
#include <Terminal.hpp>
#include <Timer.hpp>
#include <Nonce.hpp>
#include <StringOperators.hpp>
#include <util.hpp>


using EndpointType = Interface::EndpointType;

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

static bool readMessage(MessageType dstType, void *dstMessage, EndpointType srcType, MessageReader r,
	ConvertOptions const &convertOptions)
{
	Message &dst = *reinterpret_cast<Message *>(dstMessage);

	switch (srcType) {
		case EndpointType::ON_OFF_IN:
			return convertOnOffIn(dstType, dst, r.u8(), convertOptions);
		case EndpointType::TRIGGER_IN:
			return convertTriggerIn(dstType, dst, r.u8(), convertOptions);
		case EndpointType::UP_DOWN_IN:
			return convertUpDownIn(dstType, dst, r.u8(), convertOptions);
		case EndpointType::TEMPERATURE_IN:
			// convert temperature from 1/20 Kelvin to Kelvin
			if (dstType != MessageType::TEMPERATURE)
				return false; // conversion failed
			dst.temperature = float(r.u16L()) * 0.05f;
			break;
		default:
			// conversion failed
			return false;
	}

/*
	switch (dstType) {
		case MessageType::ON_OFF:
			switch (srcType) {
				case EndpointType::ON_OFF_IN: {
					uint8_t command = r.u8();
					if (command <= 2)
						dst.onOff = command;
					else
						return false; // conversion failed
				}
					break;
				case EndpointType::TRIGGER_IN:
					// trigger (button) toggles on/off
					if (r.u8() == 0)
						return false; // conversion failed
					dst.onOff = 2;
					break;
				case EndpointType::UP_DOWN_IN: {
					// up switches off, down switches on (1, 2 -> 0, 1)
					uint8_t command = r.u8();
					if (command == 0)
						return false; // conversion failed
					dst.onOff = command - 1;
				}
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		case MessageType::ON_OFF2:
			switch (srcType) {
				case EndpointType::ON_OFF_IN: {
					// invert on/off (0, 1, 2 -> 1, 0, 2)
					uint8_t command = r.u8();
					dst.onOff = command ^ 1 ^ (command >> 1);
				}
					break;
				case EndpointType::TRIGGER_IN:
					// use trigger (button) state as switch state
					dst.onOff = r.u8();
					break;
				case EndpointType::UP_DOWN_IN: {
					// up switches on, down switches off (1, 2 -> 1, 0)
					uint8_t command = r.u8();
					if (command == 0)
						return false; // conversion failed
					dst.onOff = 2 - command;
				}
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::TRIGGER:
			switch (srcType) {
				case EndpointType::TRIGGER_IN:
					dst.trigger = r.u8();
					break;
				case EndpointType::UP_DOWN_IN:
					// use up as press (0, 1 -> 0, 1)
				{
					uint8_t upDown = r.u8();
					if (upDown == 2)
						return false; // conversion failed
					dst.trigger = upDown;
				}
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		case MessageType::TRIGGER2:
			switch (srcType) {
				case EndpointType::TRIGGER_IN:
					dst.trigger = r.u8();
					break;
				case EndpointType::UP_DOWN_IN:
					// use down as press (0, 2 -> 0, 1)
				{
					uint8_t upDown = r.u8();
					if (upDown == 1)
						return false; // conversion failed
					dst.trigger = upDown >> 1;
				}
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::UP_DOWN:
			switch (srcType) {
				case EndpointType::TRIGGER_IN:
					// use press as up (0, 1 -> 0, 1)
					dst.upDown = r.u8();
					break;
				case EndpointType::UP_DOWN_IN:
					dst.trigger = r.u8();
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		case MessageType::UP_DOWN2:
			switch (srcType) {
				case EndpointType::TRIGGER_IN:
					// use press as down (0, 1 -> 0, 2)
					dst.upDown = r.u8() << 1;
					break;
				case EndpointType::UP_DOWN_IN: {
					// exchange up and down (0, 1, 2 -> 0, 2, 1)
					uint8_t command = r.u8();
					dst.upDown = ((command & 1) << 1) | (command >> 1);
				}
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::TEMPERATURE:
			switch (srcType) {
				case EndpointType::TEMPERATURE_IN:
					// convert temperature from 1/20 Kelvin to Kelvin
					dst.temperature = r.u16L() * 0.05f;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
	/ *
		case MessageType::CELSIUS:
			switch (srcType) {
			case EndpointType::TEMPERATURE_IN:
				// convert temperature from 1/20 Kelvin to Celsius
				dst.temperature = r.u16L() * 0.05f - 273.15f;
				break;
			default:
				// conversion failed
				return false;
			}
			break;
		case MessageType::FAHRENHEIT:
			switch (srcType) {
			case EndpointType::TEMPERATURE_IN:
				// convert temperature from 1/20 Kelvin to Fahrenheit
				dst.temperature = (r.u16L() * 0.05f - 273.15f) * (9.0f / 5.0f) + 32.0f;
				break;
			default:
				// conversion failed
				return false;
			}
			break;
	* /
		default:
			// conversion failed
			return false;
	}
*/
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
							EndpointType endpointType = flash.endpoints[endpointIndex];
							subscriber.barrier->resumeFirst([&subscriber, &r, endpointType](Subscriber::Parameters &p) {
								p.subscriptionIndex = subscriber.subscriptionIndex;

								// read message (note r is passed by value for multiple subscribers)
								return readMessage(subscriber.messageType, p.message, endpointType, r,
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

static bool writeMessage(MessageWriter &w, EndpointType endpointType, MessageType srcType, void const *srcMessage) {
	Message const &src = *reinterpret_cast<Message const *>(srcMessage);

	switch (endpointType) {
		case EndpointType::ON_OFF_OUT:
			return convertOnOffOut(w.u8(), srcType, src);
		case EndpointType::TRIGGER_OUT:
			return convertTriggerOut(w.u8(), srcType, src);
		case EndpointType::UP_DOWN_OUT:
			return convertUpDownOut(w.u8(), srcType, src);

		case EndpointType::LEVEL_OUT:
			switch (srcType) {
				case MessageType::LEVEL:
				case MessageType::MOVE_TO_LEVEL: {
					int level = int(src.level * 256.0f);
					bus::LevelControlCommand command = !src.level.getFlag() ? bus::LevelControlCommand::SET
						: (level >= 0 ? bus::LevelControlCommand::INCREASE : bus::LevelControlCommand::DECREASE);
					uint8_t value = clamp(abs(int(src.level * 256.0f)), 0, 255);

					if (srcType == MessageType::LEVEL) {
						w.e8<bus::LevelControlCommand>(command);
						w.u8(clamp(level, 0, 255));
					}
					if (!src.moveToLevel.move.getFlag()) {
						w.e8<bus::LevelControlCommand>(command | bus::LevelControlCommand::DURATION);
						w.u8(clamp(abs(level), 0, 255));
						w.u16L(clamp(int(src.moveToLevel.move * 10.0f), 0, 65535)); // 1/10 s
					} else {
						w.e8<bus::LevelControlCommand>(command | bus::LevelControlCommand::SPEED);
						w.u8(clamp(abs(level), 0, 255));
						w.u16L(clamp(int(src.moveToLevel.move * 5000.0f), 0, 65535)); // 1 / 5000s
					}
					break;
				}
				default:
					// conversion failed
					return false;
			}
			break;

		//case EndpointType::TEMPERATURE:
		//	break;

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
					EndpointType endpointType = device->endpoints[endpointIndex];

					if ((endpointType & EndpointType::OUT) == EndpointType::OUT) {
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
							if (!writeMessage(w, endpointType, publisher.messageType, publisher.message))
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

Array<EndpointType const> BusInterface::BusDevice::getEndpoints() const {
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
