#include "BusInterface.hpp"
#include <BusMaster.hpp>
#include <Storage.hpp>
#include <Terminal.hpp>
#include <Timer.hpp>
#include <Nonce.hpp>
#include <StringOperators.hpp>
#include <Pointer.hpp>
#include <util.hpp>
#include <appConfig.hpp>


//constexpr int maxMessageLength = 64;
constexpr int micLength = 4;

// timeout to wait for a response (e.g. default response or route reply)
constexpr SystemDuration TIMEOUT = 1s;

// number of retries when a send fails
constexpr int MAX_RETRY = 2;


// BusInterface

BusInterface::BusInterface(PersistentStateManager &stateManager)
	: securityCounter(stateManager)
{
	// load list of device ids
	int deviceCount = Storage::read(STORAGE_CONFIG, STORAGE_ID_BUS1, sizeof(this->deviceIds), this->deviceIds);

	// load devices
	int j = 0;
	for (int i = 0; i < deviceCount; ++i) {
		uint8_t id = this->deviceIds[i];

		// determine size
		int size = Storage::size(STORAGE_CONFIG, STORAGE_ID_BUS1 | id);
		if (size < sizeof(DeviceData))
			continue;

		// allocate and read
		auto endpointData = reinterpret_cast<EndpointData *>(malloc(size));
		Storage::read(STORAGE_CONFIG, STORAGE_ID_BUS1 | id, size, endpointData);

		// check id
		if (endpointData->id != id) {
			// id is inconsistent, delete device
			free(endpointData);
			continue;
		}

		// check size
		if (endpointData->size() != size) {
			// size is inconsistent, delete device
			free(endpointData);
			continue;
		}

		// get or load the device this endpoint belongs to
		auto *device = getOrLoadDevice(endpointData->deviceId);
		if (device == nullptr) {
			// endpoint is orphaned
			free(endpointData);
			continue;
		}
		new Endpoint(device, endpointData);

		// device was correctly loaded
		this->deviceIds[j] = id;
		++j;
	}
	this->deviceCount = j;
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
	this->commissioning = enabled && this->deviceCount < MAX_DEVICE_COUNT;
	if (this->commissioning) {
	} else {
		// cancel current association coroutine if it is still running
		this->commissionCoroutine.cancel();
	}
}

Array<uint8_t const> BusInterface::getDeviceIds() {
	return {this->deviceCount, this->deviceIds};
}

Interface::Device *BusInterface::getDevice(uint8_t id) {
	auto device = this->devices;
	while (device != nullptr) {
		// iterate over endpoints, each endpoint is exposed as a device
		auto endpoint = device->endpoints;
		while (endpoint != nullptr) {
			if (endpoint->data->id == id)
				return endpoint;
			endpoint = endpoint->next;
		}
		device = device->next;
	}
	return nullptr;
}

void BusInterface::eraseDevice(uint8_t id) {
	auto d = &this->devices;
	while (*d != nullptr) {
		auto device = *d;

		// iterate over endpoints
		auto e = &device->endpoints;
		while (*e != nullptr) {
			auto endpoint = *e;
			if (endpoint->data->id == id) {
				// remove endpoint from linked list
				*e = endpoint->next;

				// check if device has no endpoints left
				if (device->endpoints == nullptr) {
					// remove device from linked list
					*d = device->next;

					// erase device from flash
					Storage::erase(STORAGE_CONFIG, STORAGE_ID_BUS2 | device->data.id);

					// delete device
					delete device;
				}

				// erase endpoint from flash
				Storage::erase(STORAGE_CONFIG, STORAGE_ID_BUS1 | id);

				// delete endpoint
				delete endpoint;

				goto list;
			}
		}
		d = &device->next;
	}

list:

	// erase from list of device id's
	int j = 0;
	for (int i = 0; i < this->deviceCount; ++i) {
		uint8_t id2 = this->deviceIds[i];
		if (id2 != id) {
			this->deviceIds[j] = id;
			++j;
		}
	}
	this->deviceCount = j;
	Storage::write(STORAGE_CONFIG, STORAGE_ID_BUS1, j, this->deviceIds);
}

// private:

// Endpoint
BusInterface::Endpoint::~Endpoint() {
	free(data);
}

uint8_t BusInterface::Endpoint::getId() const {
	return this->data->id;
}

String BusInterface::Endpoint::getName() const {
	return String(this->data->name);
}

void BusInterface::Endpoint::setName(String name) {
	assign(this->data->name, name);

	// write to flash
	Storage::write(STORAGE_CONFIG, STORAGE_ID_BUS1 | this->data->id, this->data->size(), this->data);
}

Array<MessageType const> BusInterface::Endpoint::getPlugs() const {
	return {this->data->plugCount, this->data->plugs};
}

void BusInterface::Endpoint::subscribe(uint8_t plugIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.plugIndex = plugIndex;
	this->subscribers.add(subscriber);
}

PublishInfo BusInterface::Endpoint::getPublishInfo(uint8_t plugIndex) {
	if (plugIndex >= this->data->plugCount)
		return {};
	return {{.type = this->data->plugs[plugIndex], .device = {this->data->id, plugIndex}},
		&this->device->interface->publishBarrier};
}


uint8_t BusInterface::allocateId(int deviceCount) {
	// find a free id
	int id;
	for (id = 1; id < 256; ++id) {
		for (int j = 0; j < deviceCount; ++j) {
			if (this->deviceIds[j] == id)
				goto found;
		}
		break;
	found:;
	}
	return id;
}

uint8_t BusInterface::allocateDeviceId() {
	// find a free id
	int id;
	for (id = 0; id < 256; ++id) {
		auto device = this->devices;
		while (device != nullptr) {
			if (device->data.id == id)
				goto found;
			device = device->next;
		}
		break;
	found:;
	}
	return id;
}

BusInterface::BusDevice *BusInterface::getOrLoadDevice(uint8_t id) {
	auto device = this->devices;
	while (device != nullptr) {
		if (device->data.id == id)
			return device;
		device = device->next;
	}

	// load data
	DeviceData data;
	if (Storage::read(STORAGE_CONFIG, STORAGE_ID_BUS2 | id, sizeof(data), &data) != sizeof(data))
		return nullptr;

	// check id
	if (data.id != id)
		return nullptr;

	// create device
	return new BusDevice(this, data);
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
	/*case MessageType::LEVEL: {
		// level: convert from 0 - 65535 to 0.0 - 1.0
		float level = float(r.u16L()) / 65535.0f;
		if ((srcType & MessageType::CMD) == 0)
			return convertFloat(dstType, dst, level, convertOptions);
		uint8_t command = r.u8();
		if (command > 1) {
			// negative step
			command = 1;
			level = -level;
		}
		return convertFloatCommand(dstType, dst, command, level, convertOptions);
	}*/
	case MessageType::LEVEL:
	case MessageType::PHYSICAL:
	case MessageType::CONCENTRATION: {
		float value = r.f32L();
		if ((srcType & MessageType::CMD) == 0)
			return convertFloat(dstType, dst, value, convertOptions);
		uint8_t command = r.u8();
		return convertFloatCommand(dstType, dst, value, command, convertOptions);
	}
	case MessageType::LIGHTING: {
		float value = r.f32L();
		if ((srcType & MessageType::CMD) == 0)
			return convertFloat(dstType, dst, value, convertOptions);
		uint8_t command = r.u8();
		uint16_t transition = r.u16L();
		return convertFloatTransition(dstType, dst, value, command, transition, convertOptions);
/*
		// lighting: all values are normalized except color temperature
		float value = r.u16L();
		if ((srcType & MessageType::LIGHTING_CATEGORY) != MessageType::LIGHTING_COLOR_TEMPERATURE)
			value /= 65535.0f;
		if ((srcType & MessageType::CMD) == 0)
			return convertFloat(dstType, dst, value, convertOptions);
		uint8_t command = r.u8();
		uint16_t transition = r.u16L();
		return convertFloatTransition(dstType, dst, value, command, transition, convertOptions);
*/
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
	uint8_t receiveMessage[MESSAGE_LENGTH];

	while (true) {
		int receiveLength = MESSAGE_LENGTH;
		co_await BusMaster::receive(receiveLength, receiveMessage);

		// debug print received message
		//for (int i = 0; i < receiveLength; ++i)
		//	Terminal::out << hex(receiveMessage[i]) << ' ';
		//Terminal::out << '\n';

		bus::MessageReader r(receiveLength, receiveMessage);

		// set start of header
		r.setHeader();

		// get address
		if (r.peekU8() == 0) {
			// 0: enumerate message
			r.u8();
			if (!this->commissioning)
				continue;

			// get encoded device id
			uint32_t deviceId = r.id();

			// check message integrity code (mic)
			r.setMessageFromEnd(micLength);
			Nonce nonce(deviceId, 0);
			if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
				continue;

			// protocol version
			auto version = r.u8();

			// endpoint count
			auto endpointCount = r.u8();

			// cancel current association coroutine
			this->commissionCoroutine.cancel();

			this->commissionCoroutine = handleCommission(deviceId, endpointCount);
		} else {
			// data message

			// get address
			uint8_t address = r.address();

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
			if (endpointIndex == 255) {
				endpointIndex = r.u8();

				// check if there is a device in the commissioning process
				if (this->commissionCoroutine.isAlive() && this->tempDevice->data.id == address) {
					auto attribute = r.e8<bus::Attribute>();
					int length = min(r.getRemaining(), MESSAGE_LENGTH);
					uint8_t *response = r.current;
					this->responseBarrier.resumeFirst(
						[length, response, address, endpointIndex, attribute](Response &r) {
							if (r.address == address && r.endpointIndex == endpointIndex && r.attribute == attribute) {
								r.length = length;
								array::copy(length, r.response, response);
								return true;
							}
							return false;
						});
				}
			} else {
				// search device with address
				auto device = this->devices;
				while (device != nullptr) {
					if (device->data.id != address) {
						device = device->next;
						continue;
					}

					// search endpoint with index
					auto endpoint = device->endpoints;
					while (endpoint != nullptr) {
						if (endpointIndex > 0) {
							--endpointIndex;
							endpoint = endpoint->next;
							continue;
						}

						// get plug index
						uint8_t plugIndex = r.u8();

						// publish to subscribers
						for (auto &subscriber: endpoint->subscribers) {
							// check if this is the right endpoint
							if (subscriber.source.device.plugIndex == plugIndex) {
								auto srcType = endpoint->data->plugs[plugIndex];
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
					break;
				}
			}
		}
	}
}

AwaitableCoroutine BusInterface::handleCommission(uint32_t deviceId, uint8_t endpointCount) {
	int length;
	uint8_t message[MESSAGE_LENGTH];

	// find existing device if any
	auto od = &this->devices;
	while (*od != nullptr) {
		auto device = *od;
		if (device->data.deviceId == deviceId)
			break;
		od = &device->next;
	}
	auto oldDevice = *od;

	// create new device
	DeviceData deviceData;
	deviceData.id = oldDevice != nullptr ? oldDevice->data.id : allocateDeviceId();
	deviceData.deviceId = deviceId;

	// use pointer in case the coroutine gets cancelled
	Pointer<BusDevice> device = new BusDevice(deviceData);

	// set pointer so that we can forward responses
	this->tempDevice = device.ptr;

	// send commissioning message
	{
		bus::MessageWriter w(message);

		// set start of header
		w.setHeader();

		// command prefix
		w.u8(0);

		// arbitration byte that always "wins" against enumeration messages sent by devices
		w.u8(0);

		// device id
		w.u32L(deviceId);

		// address that gets assigned to the device (use storage id)
		w.u8(deviceData.id);

		// key
		w.data8(*this->key);

		// only add message integrity code (mic) using default key, message stays unencrypted
		w.setMessage();
		Nonce nonce(deviceId, 0);
		w.encrypt(micLength, nonce, bus::defaultAesKey);

		length = w.getLength();
	}
	co_await BusMaster::send(length, message);


	// configure endpoints
	Endpoint *oldEndpoint = oldDevice != nullptr ? oldDevice->endpoints : nullptr;
	int deviceCount = this->deviceCount;
	for (uint8_t endpointIndex = 0; endpointIndex < endpointCount; ++endpointIndex) {
		// get plug list
		co_await readAttribute(length, message, *device, endpointIndex, bus::Attribute::PLUG_LIST);
		if (length == -1)
			co_return;
		int plugCount = length / 2;

		// allocate endpoint data
		auto data = reinterpret_cast<EndpointData *>(malloc(offsetOf(EndpointData, plugs[plugCount])));

		// set plugs
		data->plugCount = plugCount;
		MessageReader plugs(length, message);
		for (int i = 0; i < plugCount; ++i) {
			data->plugs[i] = plugs.e16L<MessageType>();
			Terminal::out << dec(endpointIndex) << " " << getTypeLabel(data->plugs[i]) << "\n";
		}

		// set id
		if (oldEndpoint != nullptr) {
			data->id = oldEndpoint->data->id;
		} else {
			data->id = allocateId(deviceCount);
			this->deviceIds[deviceCount++] = data->id;
		}

		// set device id
		data->deviceId = device->data.id;

		// set name
		co_await readAttribute(length, message, *device, endpointIndex, bus::Attribute::MODEL_IDENTIFIER);
		if (length > 0)
			assign(data->name, String(length, message));
		else
			assign(data->name, "Device");

		// create endpoint, device takes ownership
		auto endpoint = new Endpoint(device.ptr, data);

		if (oldEndpoint != nullptr)
			oldEndpoint = oldEndpoint->next;
	}

	// remove remaining old endpoints from device list
	while (oldEndpoint != nullptr) {
		int j = 0;
		for (int i = 0; i < deviceCount; ++i) {
			auto id = this->deviceIds[i];
			if (id != oldEndpoint->data->id)
				this->deviceIds[j++] = id;
		}
		deviceCount = j;
		oldEndpoint = oldEndpoint->next;
	}

	// delete old device and its endpoints
	if (oldDevice != nullptr) {
		// remove device from linked list
		*od = oldDevice->next;

		// delete old endpoints
		auto endpoint = device->endpoints;
		auto oldEndpoint = oldDevice->endpoints;
		while (oldEndpoint != nullptr) {
			// move subscribers from old to new endpoint
			if (endpoint != nullptr) {
				endpoint->subscribers.insert(oldEndpoint->subscribers);
				endpoint = endpoint->next;
			}
			auto e = oldEndpoint;
			oldEndpoint = oldEndpoint->next;
			delete e;
		}
		delete oldDevice;
	}

	// store device list
	if (this->deviceCount != deviceCount) {
		this->deviceCount = deviceCount;
		Storage::write(STORAGE_CONFIG, STORAGE_ID_BUS1, deviceCount, this->deviceIds);
	}

	// store endpoints
	auto endpoint = device->endpoints;
	while (endpoint != nullptr) {
		Storage::write(STORAGE_CONFIG, STORAGE_ID_BUS1 | endpoint->data->id, endpoint->data->size(), endpoint->data);
		endpoint = endpoint->next;
	}

	// store device
	Storage::write(STORAGE_CONFIG, STORAGE_ID_BUS2 | device->data.id, sizeof(device->data), &device->data);

	// transfer ownership of device to devices list
	device->interface = this;
	device->next = this->devices;
	this->devices = device.ptr;
	device.ptr = nullptr;
}

AwaitableCoroutine BusInterface::readAttribute(int &length, uint8_t (&message)[MESSAGE_LENGTH], BusDevice &device,
	uint8_t endpointIndex, bus::Attribute attribute)
{
	for (int retry = 0; ; ++retry) {
		// request attribute
		{
			bus::MessageWriter w(message);

			// set start of header
			w.setHeader();

			// encoded address
			w.address(device.data.id);

			// security counter
			w.u32L(this->securityCounter);

			// set start of message
			w.setMessage();

			// "escaped" endpoint index
			w.u8(255);
			w.u8(endpointIndex);

			// attribute
			w.e8(attribute);

			// no message data to indicate that we want to read the attribute

			// encrypt
			Nonce nonce(device.data.id, this->securityCounter);
			w.encrypt(micLength, nonce, *this->aesKey);

			// increment security counter
			++this->securityCounter;

			length = w.getLength();

		}
		co_await BusMaster::send(length, message);

		// wait for a response from the device
		int r = co_await select(
			this->responseBarrier.wait(length, message, device.data.id, endpointIndex, attribute),
			Timer::sleep(TIMEOUT));

		// check if response was received
		if (r == 1)
			break;

		if (retry == MAX_RETRY) {
			length = -1;
			co_return;
		}
	}
}

static bool writeMessage(MessageWriter &w, MessageType type, Message &message) {
	switch (type & MessageType::CATEGORY) {
	case bus::PlugType::BINARY:
	case bus::PlugType::TERNARY:
		w.u8(message.value.u8);
		break;
	case MessageType::MULTISTATE:
		// todo
		break;
	/*case MessageType::LEVEL: {
		float value = message.value.f;
		uint8_t cmd = 0;
		if ((type & MessageType::CMD) != 0) {
			cmd = message.command;
			if (value < 0 && cmd != 0) {
				value = -value;
				cmd = 2;
			}
		}
		w.u16L(clamp(int(value * 65535.0f + 0.5f), 0, 65535));
		if ((type & MessageType::CMD) != 0)
			w.u8(cmd);
		break;
	}*/
	case MessageType::LEVEL:
	case MessageType::PHYSICAL:
	case MessageType::CONCENTRATION:
		w.f32L(message.value.f);
		if ((type & MessageType::CMD) != 0)
			w.u8(message.command);
		break;
	case MessageType::LIGHTING: {
		w.f32L(message.value.f);
		if ((type & MessageType::CMD) != 0) {
			w.u8(message.command);
			w.u16L(message.transition);
		}
		break;
/*
		float value = message.value.f;
		if ((type & MessageType::LIGHTING_CATEGORY) != MessageType::LIGHTING_COLOR_TEMPERATURE)
			value *= 65535.0f;
		w.u16L(clamp(int(value + 0.5f), 0, 65535));
		if ((type & MessageType::CMD) != 0) {
			w.u8(message.command);
			w.u16L(message.transition);
		}
		break;*/
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
	uint8_t sendMessage[MESSAGE_LENGTH];
	while (true) {
		// wait for message
		MessageInfo info;
		Message message;
		co_await this->publishBarrier.wait(info, &message);

		// find destination device
		auto device = this->devices;
		while (device != nullptr) {
			auto endpoint = device->endpoints;
			int endpointIndex = 0;
			while (endpoint != nullptr) {
				if (endpoint->data->id != info.device.id) {
					endpoint = endpoint->next;
					++endpointIndex;
					continue;
				}

				// get endpoint info
				uint8_t plugIndex = info.device.plugIndex;
				if (plugIndex >= endpoint->data->plugCount)
					break;
				auto messageType = endpoint->data->plugs[plugIndex];

				// check if it is an output
				if (isInput(messageType)) {
					// send data message to node
					int length;
					{
						bus::MessageWriter w(sendMessage);

						// set start of header
						w.setHeader();

						// encoded device address
						w.address(device->data.id);

						// security counter
						w.u32L(this->securityCounter);

						// set start of message
						w.setMessage();

						// endpoint index
						w.u8(endpointIndex);

						// plug index
						w.u8(plugIndex);

						// message data
						if (!writeMessage(w, messageType, message))
							break;

						// encrypt
						Nonce nonce(device->data.id, this->securityCounter);
						w.encrypt(micLength, nonce, *this->aesKey);

						// increment security counter
						++this->securityCounter;
						length = w.getLength();
					}
					co_await BusMaster::send(length, sendMessage);
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
			if (endpoint != nullptr)
				break;
			device = device->next;
		}
	}
}
