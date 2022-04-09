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
	: securityCounter(stateManager)
{
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

DeviceId BusInterface::getDeviceId(int index) {
	assert(index >= 0 && index < this->devices.count());
	return this->devices[index]->deviceId;
}

Array<Interface::EndpointType const> BusInterface::getEndpoints(DeviceId deviceId) {
	for (auto &device : this->devices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId) {
			return {flash.endpointCount, flash.endpoints};
		}
	}
	return {};
}

void BusInterface::addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) {
	for (auto &device : this->devices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId && endpointIndex < flash.endpointCount) {
			publisher.remove();
			publisher.index = endpointIndex;
			publisher.event = &this->publishEvent;
			device.publishers.add(publisher);
			break;
		}
	}
}

void BusInterface::addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) {
	for (auto &device : this->devices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId/* && endpointIndex < flash.endpointCount*/) {
			subscriber.remove();
			subscriber.index = endpointIndex;
			device.subscribers.add(subscriber);
			break;
		}
	}
}

bool readMessage(MessageType dstType, void *dstMessage, MessageReader r, EndpointType srcType) {
	Message &dst = *reinterpret_cast<Message *>(dstMessage);

	switch (dstType) {
	case MessageType::UNKNOWN:
		return false;

	case MessageType::ON_OFF:
		switch (srcType) {
		case EndpointType::ON_OFF_IN:
			{
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
		case EndpointType::UP_DOWN_IN:
			{
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
		case EndpointType::ON_OFF_IN:
			{
				// invert on/off (0, 1, 2 -> 1, 0, 2)
				uint8_t command = r.u8();
				dst.onOff = command ^ 1 ^ (command >> 1);
			}
			break;
		case EndpointType::TRIGGER_IN:
			// use trigger (button) state as switch state
			dst.onOff = r.u8();
			break;
		case EndpointType::UP_DOWN_IN:
			{
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
		case EndpointType::UP_DOWN_IN:
			{
				// exchange up and down (0, 1, 2 -> 0, 2, 1)
				uint8_t command = r.u8();
				dst.upDown = (command << 1) | (command >> 1);
			}
			break;
		default:
			// conversion failed
			return false;
		}
		break;

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

	default:
		// conversion failed
		return false;
	}
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

			// get endpoints
			flash.endpointCount = r.getRemaining();
			array::copy(flash.endpointCount, flash.endpoints, r.current);

			// commission the device
			constexpr int commissionLength = 1 + 1 + 4 + 1 + 16 + micLength; // command prefix, arbitration, device id, address, key, mic
			uint8_t sendMessage[commissionLength];

			// check if device already exists and set flag for each used address to find a free address
			array::fill(9, sendMessage, 0);
			int index = this->devices.count();
			for (int i = 0; i < this->devices.count(); ++i) {
				if (this->devices[i]->deviceId == deviceId)
					index = i;
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
				Device* device = new Device(flash);
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
			if (!r.decrypt(micLength, nonce, *this->aesKey))
				continue;

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
								return readMessage(subscriber.messageType, p.message, r, endpointType);
							});
						}
					}
					break;
				}
			}
		}
	}
}

bool writeMessage(MessageWriter &w, EndpointType endpointType, MessageType srcType, void const *srcMessage) {
	Message const &src = *reinterpret_cast<Message const *>(srcMessage);

	switch (endpointType & EndpointType::TYPE_MASK) {
	case EndpointType::ON_OFF:
		switch (srcType) {
		case MessageType::ON_OFF:
			w.u8(src.onOff);
			break;
		case MessageType::ON_OFF2:
			// invert on/off (0, 1, 2 -> 1, 0, 2)
			w.u8(src.onOff ^ 1 ^ (src.onOff >> 1));
			break;
		case MessageType::TRIGGER:
		case MessageType::TRIGGER2:
			// trigger (e.g.button) toggles on/off
			if (src.trigger == 0)
				return false;
			w.u8(2);
			break;
		case MessageType::UP_DOWN:
			// rocker switches off/on (1, 2 -> 0, 1)
			if (src.upDown == 0)
				return false;
			w.u8(src.upDown - 1);
			break;
		case MessageType::UP_DOWN2:
			// rocker switches on/off (1, 2 -> 1, 0)
			if (src.upDown == 0)
				return false;
			w.u8(2 - src.upDown);
			break;
		default:
			// conversion failed
			return false;
		}
		break;

	case EndpointType::UP_DOWN:
		switch (srcType) {
		case MessageType::TRIGGER:
			// use press as up (0, 1 -> 0, 1)
			w.u8(src.trigger);
			break;
		case MessageType::TRIGGER2:
			// use press as down (0, 1 -> 0, 2)
			w.u8(src.trigger << 1);
			break;
		case MessageType::UP_DOWN:
			w.u8(src.upDown);
			break;
		case MessageType::UP_DOWN2:
			// invert up/down (0, 1, 2 -> 1, 0, 2)
			//w.u8(src.upDown ^ 1 ^ (src.upDown >> 1));
			// invert up/down (0, 1, 2 -> 0, 2, 1)
			w.u8((src.upDown << 1) | (src.upDown >> 1));
		default:
			// conversion failed
			return false;
		}
		break;

	case EndpointType::LEVEL:
		switch (srcType) {
		case MessageType::LEVEL:
		case MessageType::MOVE_TO_LEVEL:
			{
				int level = int(src.level * 256.0f);
				bus::LevelControlCommand command = !src.level.getFlag() ? bus::LevelControlCommand::SET
					: (level >= 0 ? bus::LevelControlCommand::INCREASE : bus::LevelControlCommand::DECREASE);
				uint8_t value = clamp(abs(int(src.level * 256.0f)), 0, 255);

				if (srcType == MessageType::LEVEL) {
					w.e8<bus::LevelControlCommand>(command);
					w.u8(clamp(level, 0, 255));
				} if (!src.moveToLevel.move.getFlag()) {
					w.e8<bus::LevelControlCommand>(command | bus::LevelControlCommand::DURATION);
					w.u8(clamp(abs(level), 0, 255));
					w.u16L(clamp(int(src.moveToLevel.move * 10.0f), 0, 65535)); // 1/10 s
				} else {
					w.e8<bus::LevelControlCommand>(command | bus::LevelControlCommand::SPEED);
					w.u8(clamp(abs(level), 0, 255));
					w.u16L(clamp(int(src.moveToLevel.move * 5000.0f), 0, 65535)); // 1 / 5000s
				}
			}
			break;
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
		for (auto &device : this->devices) {
			// iterate over publishers
			for (auto &publisher : device.publishers) {
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

					// forward to subscribers
					for (auto &subscriber : device.subscribers) {
						if (subscriber.index == publisher.index) {
							subscriber.barrier->resumeAll([&subscriber, &publisher] (Subscriber::Parameters &p) {
								p.subscriptionIndex = subscriber.subscriptionIndex;

								// convert to target unit and type and resume coroutine if conversion was successful
								return convert(subscriber.messageType, p.message,
									publisher.messageType, publisher.message);
							});
						}
					}
				}
			}
		}
	}
}


// BusInterface::DeviceFlash

int BusInterface::DeviceFlash::size() const {
	return getOffset(DeviceFlash, endpoints[this->endpointCount]);
}

BusInterface::Device *BusInterface::DeviceFlash::allocate() const {
	return new Device(*this);
}
