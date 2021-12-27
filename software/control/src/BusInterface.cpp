#include "BusInterface.hpp"
#include <util.hpp>
#include <Nonce.hpp>
#include <bus.hpp>
#include <sys.hpp>
#include <timer.hpp>


AesKey const busDefaultAesKey = {{0x1337c6b3, 0xf16c7cb6, 0x2dec182d, 0x3078d618, 0xaec16bb7, 0x5fad1701, 0x72410f2c, 0x4239d934, 0xbef4739b, 0xe159649a, 0x93186bb6, 0xd121b282, 0x47c360a5, 0xa69a043f, 0x35826f89, 0xe4a3dd0b, 0x45024bcc, 0xe3984ff3, 0xd61a207a, 0x32b9fd71, 0x0356e8ef, 0xe0cea71c, 0x36d48766, 0x046d7a17, 0x1f8c181d, 0xff42bf01, 0xc9963867, 0xcdfb4270, 0x50a049a0, 0xafe2f6a1, 0x6674cec6, 0xab8f8cb6, 0xa3c407c2, 0x0c26f163, 0x6a523fa5, 0xc1ddb313, 0x79a97aba, 0x758f8bd9, 0x1fddb47c, 0xde00076f, 0x2c6cd2a7, 0x59e3597e, 0x463eed02, 0x983eea6d}};

constexpr int minMessageLength = 2 + 4 + 1 + 4; // ecoded address, security counter, endpoint index, no payload, mic
constexpr int maxMessageLength = minMessageLength + 8; // payload
constexpr int micLength = 4;

// number of retries when a send fails
constexpr int MAX_RETRY = 4;


// BusInterface::MessageReader

uint8_t BusInterface::MessageReader::arbiter() {
	uint8_t value = uint8();

	uint8_t count = 0;
	while (value > 0) {
		++count;
		value <<= 1;
	}
	return count;
}


// BusInterface::MessageWriter

void BusInterface::MessageWriter::arbiter(uint8_t value) {
	uint8(~(0xff >> value));
}


// BusInterface

BusInterface::BusInterface(Configuration &configuration, PersistentStateManager &stateManager)
	: configuration(configuration)
	, securityCounter(configuration->busSecurityCounterOffset)
{
	init(stateManager);
}

BusInterface::~BusInterface() {

}

void BusInterface::setCommissioning(bool enabled) {
	if (enabled) {
		// start commissioning coroutine that polls new devices
		this->commissionCoroutine = commission();
	} else {
		this->commission().cancel();
	}
}

int BusInterface::getDeviceCount() {
	return this->devices.count();
}

DeviceId BusInterface::getDeviceId(int index) {
	assert(index >= 0 && index < this->devices.count());
	return this->devices[index]->deviceId;
}

Array<EndpointType const> BusInterface::getEndpoints(DeviceId deviceId) {
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
		if (flash.deviceId == deviceId && endpointIndex < flash.endpointCount) {
			subscriber.remove();
			subscriber.index = endpointIndex;
			device.subscribers.add(subscriber);
			break;
		}
	}
}

Coroutine BusInterface::init(PersistentStateManager &stateManager) {
	// restore security counter
	co_await this->securityCounter.restore(&stateManager);
		
	// start coroutines
	publish();
	awaitRequest();
}

AwaitableCoroutine BusInterface::commission() {
	constexpr int minEnumerateLength = 1 + 11 + 4; // command prefix, encoded device id, mic
	constexpr int maxEnumerateLength = minEnumerateLength + DeviceFlash::MAX_ENDPOINT_COUNT; // endpoints
	constexpr int commissionLength = 1 + 1 + 4 + 1 + 16 + 4; // command prefix, arbitration, device id, address, key, mic

	uint8_t inMessage[maxEnumerateLength];
	uint8_t outMessage[commissionLength];

	// periodically enumerate new devices
	while (true) {
		DeviceFlash flash;

		co_await timer::sleep(1s);

		// write zero as command prefix to bus and check if a device gets enumerated
		outMessage[0] = 0;
		int readLength = maxEnumerateLength;
		co_await bus::transfer(1, outMessage, readLength, inMessage);
		if (readLength < minEnumerateLength)
			continue;
		
		// get device id from enumerate response
		{
			MessageReader r(readLength, inMessage);
			
			// set start of header
			r.setHeader();
			// check if zero was looped back
			if (r.uint8() != 0)
				continue;

			// check message integrity code
			r.setMessage(r.end - micLength);
			Nonce nonce(0, 0);
			if (!r.decrypt(micLength, nonce, busDefaultAesKey))
				continue;

			// get device id
			uint32_t deviceId = 0;
			for (int i = 0; i < 11; ++i) {
				deviceId |= (r.arbiter() - 1) << i * 3;
			}
			flash.deviceId = deviceId;
			
			// get endpoints
			flash.endpointCount = r.getRemaining();
			array::copy(flash.endpointCount, flash.endpoints, r.current);
		}
		
		// check if device already exists and set used address flags
		array::fill(9, outMessage, 0);
		int index = this->devices.count();
		for (int i = 0; i < this->devices.count(); ++i) {
			if (this->devices[i]->deviceId == flash.deviceId)
				index = i;
			else
				outMessage[i >> 3] |= 1 << (i & 7);
		}
		
		// find free address
		int j;
		for (j = 0; j < 9; ++j) {
			uint8_t f = outMessage[j];
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
sys::out.write("bus device " + hex(flash.deviceId) + ": assigned address " + dec(flash.address) + '\n');

		// commission device
		{
			// get global configuration
			auto const &configuration = *this->configuration;

			MessageWriter w(outMessage);

			// set start of header
			w.setHeader();

			// command prefix
			w.uint8(0);

			// arbitration byte that always "wins" against enumeration messages sent by devices
			w.uint8(0);

			// device id
			w.uint32(flash.deviceId);
			
			// address that gets assigned to the device
			w.uint8(flash.address);
			
			// key
			w.data(configuration.key);

			// set start of message (is empty, everything is in the header)
			w.setMessage();

			// encrypt
			Nonce nonce(0, 0);
			w.encrypt(micLength, nonce, busDefaultAesKey);
			
			readLength = w.getLength();
		}
		co_await bus::transfer(readLength, outMessage, readLength, inMessage);

		Device* device = new Device(flash);

		// write the configured device to flash
		this->devices.write(index, device);
	}
}

bool readMessage(MessageType dstType, void *dstMessage, DataReader r, EndpointType srcType) {
	Message &dst = *reinterpret_cast<Message *>(dstMessage);

	switch (dstType) {
	case MessageType::UNKNOWN:
		return false;

	case MessageType::ON_OFF:
		switch (srcType) {
		case EndpointType::ON_OFF_IN:
			{
				uint8_t command = r.uint8();
				if (command <= 2)
					dst.onOff = command;
				else
					return false; // conversion failed
			}
			break;
		case EndpointType::TRIGGER_IN:
			// trigger (button) toggles on/off
			if (r.uint8() == 0)
				return false; // conversion failed
			dst.onOff = 2;
			break;
		case EndpointType::UP_DOWN_IN:
			{
				// up switches off, down switches on (1, 2 -> 0, 1)
				uint8_t command = r.uint8();
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
				uint8_t command = r.uint8();
				dst.onOff = command ^ 1 ^ (command >> 1);
			}
			break;
		case EndpointType::TRIGGER_IN:
			// use trigger (button) state as switch state
			dst.onOff = r.uint8();
			break;
		case EndpointType::UP_DOWN_IN:
			{
				// up switches on, down switches off (1, 2 -> 1, 0)
				uint8_t command = r.uint8();
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
			dst.trigger = r.uint8();
			break;
		case EndpointType::UP_DOWN_IN:
			// use up as press (0, 1 -> 0, 1)
			{
				uint8_t upDown = r.uint8();
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
			dst.trigger = r.uint8();
			break;
		case EndpointType::UP_DOWN_IN:
			// use down as press (0, 2 -> 0, 1)
			{
				uint8_t upDown = r.uint8();
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
			dst.upDown = r.uint8();
			break;
		case EndpointType::UP_DOWN_IN:
			dst.trigger = r.uint8();
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
			dst.upDown = r.uint8() << 1;
			break;
		case EndpointType::UP_DOWN_IN:
			{
				// exchange up and down (0, 1, 2 -> 0, 2, 1)
				uint8_t command = r.uint8();
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
			dst.temperature = r.uint16() * 0.05f - 273.15f;
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
			dst.temperature = (r.uint16() * 0.05f - 273.15f) * (9.0f / 5.0f) + 32.0f;
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

bool BusInterface::receive(MessageReader &r) {
	// set start of header
	r.setHeader();

	// get address
	uint8_t a1 = r.arbiter() - 1;
	uint8_t address = a1 | (r.arbiter() << 3);

	// get security counter
	uint32_t securityCounter = r.int32();

	// set start of encrypted message
	r.setMessage();

	// decrypt
	Nonce nonce(address, securityCounter);
	if (!r.decrypt(micLength, nonce, this->configuration->aesKey))
		return false;

	for (auto &device : this->devices) {
		auto const &flash = *device;
		if (flash.address == address) {
			uint8_t endpointIndex = r.uint8();
			
			// publish to subscribers
			for (auto &subscriber : device.subscribers) {
				// check if this is the right endpoint
				if (subscriber.index == endpointIndex) {
					EndpointType endpointType = flash.endpoints[endpointIndex];
					subscriber.barrier->resumeFirst([&subscriber, &r, endpointType] (Subscriber::Parameters &p) {
						p.subscriptionIndex = subscriber.subscriptionIndex;

						// read message (note r is passed by value for multiple subscribers)
						return readMessage(subscriber.messageType, p.message, r, endpointType);
					});
				}
			}
			break;
		}
	}
	return true;
}

Coroutine BusInterface::awaitRequest() {
	uint8_t inMessage[maxMessageLength];
	while (true) {
		// wait for a request by a device
		co_await bus::request();
	
		int readLength = maxMessageLength;
		co_await bus::transfer(0, nullptr, readLength, inMessage);

		if (readLength >= minMessageLength) {
			MessageReader r(readLength, inMessage);
			receive(r);
		}
	}
}

bool writeMessage(BusInterface::MessageWriter &w, EndpointType endpointType,
	MessageType srcType, void const *srcMessage)
{
	Message const &src = *reinterpret_cast<Message const *>(srcMessage);

	switch (endpointType & EndpointType::TYPE_MASK) {
	case EndpointType::ON_OFF:
		switch (srcType) {
		case MessageType::ON_OFF:
			w.uint8(src.onOff);
			break;
		case MessageType::ON_OFF2:
			// invert on/off (0, 1, 2 -> 1, 0, 2)
			w.uint8(src.onOff ^ 1 ^ (src.onOff >> 1));
			break;
		case MessageType::TRIGGER:
		case MessageType::TRIGGER2:
			// trigger (e.g.button) toggles on/off
			if (src.trigger == 0)
				return false;
			w.uint8(2);
			break;
		case MessageType::UP_DOWN:
			// rocker switches off/on (1, 2 -> 0, 1)
			if (src.upDown == 0)
				return false;
			w.uint8(src.upDown - 1);
			break;
		case MessageType::UP_DOWN2:
			// rocker switches on/off (1, 2 -> 1, 0)
			if (src.upDown == 0)
				return false;
			w.uint8(2 - src.upDown);
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
			w.uint8(src.trigger);
			break;
		case MessageType::TRIGGER2:
			// use press as down (0, 1 -> 0, 2)
			w.uint8(src.trigger << 1);
			break;
		case MessageType::UP_DOWN:
			w.uint8(src.upDown);
			break;
		case MessageType::UP_DOWN2:
			// invert up/down (0, 1, 2 -> 1, 0, 2)
			//w.uint8(src.upDown ^ 1 ^ (src.upDown >> 1));
			// invert up/down (0, 1, 2 -> 0, 2, 1)
			w.uint8((src.upDown << 1) | (src.upDown >> 1));
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
					w.enum8<bus::LevelControlCommand>(command);
					w.uint8(clamp(level, 0, 255));
				} if (!src.moveToLevel.move.getFlag()) {
					w.enum8<bus::LevelControlCommand>(command | bus::LevelControlCommand::DURATION);
					w.uint8(clamp(abs(level), 0, 255));
					w.uint16(clamp(int(src.moveToLevel.move * 10.0f), 0, 65535)); // 1/10 s
				} else {
					w.enum8<bus::LevelControlCommand>(command | bus::LevelControlCommand::SPEED);
					w.uint8(clamp(abs(level), 0, 255));
					w.uint16(clamp(int(src.moveToLevel.move * 5000.0f), 0, 65535)); // 1 / 5000s
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
						int retry = 0;
						do {
							// build packet
							int writeLength;
							{
								DeviceFlash const &flash = *device;

								MessageWriter w(outMessage);
								
								// set start of header
								w.setHeader();

								// encoded address
								w.arbiter((flash.address & 7) + 1);
								w.arbiter(flash.address >> 3);

								// security counter
								w.uint32(this->securityCounter);
								
								// set start of message
								w.setMessage();
								
								// endpoint index
								w.uint8(endpointIndex);

								// message data
								if (!writeMessage(w, endpointType, publisher.messageType, publisher.message))
									break;

								// encrypt
								Nonce nonce(flash.address, this->securityCounter);
								w.encrypt(micLength, nonce, this->configuration->aesKey);

								//bool ok = decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, this->configuration->networkAesKey);

								// increment security counter
								++this->securityCounter;

								writeLength = w.getLength();
							}
							int readLength = maxMessageLength;
							co_await bus::transfer(writeLength, outMessage, readLength, inMessage);
							
							// check if inMessage is same as outMessage which means send was successful
							if (readLength == writeLength && array::equals(readLength, inMessage, outMessage))
								break;
							
							// maybe a device has sent a message and has overridden our message (don't count as retry)
							if (readLength >= minMessageLength) {
								MessageReader r(readLength, inMessage);
								if (!receive(r))
									++retry;
							} else {
								++retry;
							}
						} while (retry < MAX_RETRY);
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
