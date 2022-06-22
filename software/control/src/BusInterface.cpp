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
	: securityCounter(stateManager)
{
	// set backpointers
	for (auto &device: this->devices)
		device.interface = this;
}

BusInterface::~BusInterface() {
}

void BusInterface::setConfiguration(DataBuffer<16> const &key, AesKey const &aesKey) {
	bool first = this->key == nullptr;
	this->key = &key;
	this->aesKey = &aesKey;
	if (first)
		start();
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

Coroutine BusInterface::start() {
	// restore security counter
	co_await this->securityCounter.restore();

	// start coroutines
	receive();
	for (int i = 0; i < PUBLISH_COUNT; ++i)
		publish();
}

static bool readMessage(MessageType dstType, void *dstMessage, MessageType srcType, MessageReader r,
	ConvertOptions const &convertOptions)
{
	auto &dst = *reinterpret_cast<Message *>(dstMessage);

	switch (srcType & MessageType::CATEGORY) {
	case MessageType::BINARY:
	case MessageType::TERNARY:
		return convertSwitch(dstType, dst, r.u8(), convertOptions);
	case MessageType::MULTISTATE:
		// todo
		break;
	case MessageType::LEVEL: {
		// level: convert from 0 - 65535 to 0.0 - 1.0
		float level = float(r.u16L()) / 65535.0f;
		if ((srcType & MessageType::CMD) == 0)
			return convertFloat(dstType, dst, level, convertOptions);
		uint8_t command = r.u8();
		return convertFloatCommand(dstType, dst, command, level, convertOptions);
	}
	case MessageType::PHYSICAL:
	case MessageType::CONCENTRATION: {
		float value = r.f32L();
		if ((srcType & MessageType::CMD) == 0)
			return convertFloat(dstType, dst, value, convertOptions);
		uint8_t command = r.u8();
		return convertFloatCommand(dstType, dst, command, value, convertOptions);
	}
	case MessageType::LIGHTING: {
		// lighting: all values are normalized except color temperature
		float value = r.u16L();
		if ((srcType & MessageType::LIGHTING_CATEGORY) != MessageType::LIGHTING_COLOR_TEMPERATURE)
			value /= 65535.0f;
		if ((srcType & MessageType::CMD) == 0)
			return convertFloat(dstType, dst, value, convertOptions);
		uint8_t command = r.u8();
		uint16_t transition = r.u16L();
		return convertFloatTransition(dstType, dst, value, command, transition, convertOptions);
	}
	case MessageType::METERING:
		// todo
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

		// debug print received message
		//for (int i = 0; i < receiveLength; ++i)
		//	Terminal::out << hex(receiveMessage[i]) << ' ';
		//Terminal::out << '\n';

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

			// check if a device with deviceId already exists and if yes, reuse the interfaceId
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
			flash.endpointCount = r.getRemaining() / 2;
			r.data16L(flash.endpointCount, flash.endpoints);

			// commission the device
			constexpr int commissionLength =
				1 + 1 + 4 + 1 + 16 + micLength; // command prefix, arbitration, device id, address, key, mic
			uint8_t sendMessage[commissionLength];

			// continue if no free address found
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
				w.data8(*this->key);

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

			// get address
			uint8_t address = a1 - 1 + r.arbiter() * 8;

			// search device with address
			for (auto &device: this->devices) {
				auto const &flash = *device;
				if (flash.address == address) {
					// get security counter
					uint32_t securityCounter = r.u32L();
					//Terminal::out << "device " << dec(address) << " security counter " << hex(securityCounter) << '\n';

					// set start of encrypted message
					r.setMessage();

					// decrypt
					Nonce nonce(address, securityCounter);
					if (!r.decrypt(micLength, nonce, *this->aesKey)) {
						Terminal::out << "bus: decrypt failed!\n";
						break;
					}

					// get endpoint index
					uint8_t endpointIndex = r.u8();

					// publish to subscribers
					for (auto &subscriber: device.subscribers) {
						// check if this is the right endpoint
						if (subscriber.source.device.endpointIndex == endpointIndex) {
							auto srcType = flash.endpoints[endpointIndex];
							subscriber.barrier->resumeFirst([&subscriber, &r, srcType](PublishInfo::Parameters &p) {
								p.info = subscriber.destination;

								// read message (note r is passed by value for multiple subscribers)
								return readMessage(subscriber.destination.type, p.message, srcType, r,
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

static bool writeMessage(MessageWriter &w, MessageType type, Message &message) {
	switch (type & MessageType::CATEGORY) {
	case bus::EndpointType::BINARY:
	case bus::EndpointType::TERNARY:
		w.u8(message.value.u8);
		break;
	case MessageType::MULTISTATE:
		// todo
		break;
	case MessageType::LEVEL:
		w.u16L(clamp(int(message.value.f * 65535.0f + 0.5f), 0, 65535));
		if ((type & MessageType::CMD) != 0)
			w.u8(message.command);
		break;
	case MessageType::PHYSICAL:
	case MessageType::CONCENTRATION:
		w.f32L(message.value.f);
		if ((type & MessageType::CMD) != 0)
			w.u8(message.command);
		break;
	case MessageType::LIGHTING: {
		float value = message.value.f;
		if ((type & MessageType::LIGHTING_CATEGORY) != MessageType::LIGHTING_COLOR_TEMPERATURE)
			value *= 65535.0f;
		w.u16L(clamp(int(value + 0.5f), 0, 65535));
		if ((type & MessageType::CMD) != 0) {
			w.u8(message.command);
			w.u16L(message.transition);
		}
		break;
	}
	case MessageType::METERING:
		// todo
		break;
	default:
		// not supported
		return false;
	}
	return true;
}

Coroutine BusInterface::publish() {
	uint8_t outMessage[maxMessageLength];
	uint8_t inMessage[maxMessageLength];
	while (true) {
		// wait for message
		MessageInfo info;
		Message message;
		co_await this->publishBarrier.wait(info, &message);

		// find destination device
		for (auto &device: this->devices) {
			if (device->interfaceId != info.device.id)
				continue;

			// get endpoint info
			uint8_t endpointIndex = info.device.endpointIndex;
			if (endpointIndex >= device->endpointCount)
				break;
			auto messageType = device->endpoints[endpointIndex];

			// check if it is an output
			if (isInput(messageType)) {
				// send data message to node
				bus::MessageWriter w(outMessage);
				{
					DeviceFlash const &flash = *device;

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
					if (!writeMessage(w, messageType, message))
						break;

					// encrypt
					Nonce nonce(flash.address, this->securityCounter);
					w.encrypt(micLength, nonce, *this->aesKey);

					// increment security counter
					++this->securityCounter;
				}
				co_await BusMaster::send(w.getLength(), outMessage);
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

			break;
		}
	}
}


// BusInterface::DeviceFlash

int BusInterface::DeviceFlash::size() const {
	return offsetOf(DeviceFlash, endpoints[this->endpointCount]);
}

BusInterface::BusDevice *BusInterface::DeviceFlash::allocate() const {
	return new BusDevice(*this);
}


// BusInterface::BusDevice

BusInterface::BusDevice::~BusDevice() {
}

uint8_t BusInterface::BusDevice::getId() const {
	auto &flash = **this;
	return flash.interfaceId;
}

String BusInterface::BusDevice::getName() const {
	return "b";
}

void BusInterface::BusDevice::setName(String name) {

}

Array<MessageType const> BusInterface::BusDevice::getEndpoints() const {
	auto &flash = **this;
	return {flash.endpointCount, flash.endpoints};
}

void BusInterface::BusDevice::subscribe(uint8_t endpointIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.endpointIndex = endpointIndex;
	this->subscribers.add(subscriber);
}

PublishInfo BusInterface::BusDevice::getPublishInfo(uint8_t endpointIndex) {
	auto &flash = **this;
	if (endpointIndex >= flash.endpointCount)
		return {};
	return {{.type = flash.endpoints[endpointIndex], .device = {flash.interfaceId, endpointIndex}},
		&this->interface->publishBarrier};
}
