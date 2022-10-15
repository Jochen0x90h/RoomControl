#pragma once

#include "Interface.hpp"
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <Storage.hpp>
#include <appConfig.hpp>


/**
 * Devices connected by bus using LIN physical layer
 */
class BusInterface : public Interface {
public:
	// maximum number of elements that can be commissioned (each device endpoint is one element)
	static constexpr int MAX_ELEMENT_COUNT = 128;

	// last valid bus device address
	static constexpr int LAST_ADDRESS = 8 * 9 - 1;

	// number of coroutines for receiving and publishing
	static constexpr int RECEIVE_COUNT = 4;
	static constexpr int PUBLISH_COUNT = 4;

	/**
	 * Constructor
	 * @param interfaceId id of interface
	 * @param storage persistent storage for device configuration
	 * @param counters persistent storage for security counters
	 */
	BusInterface(uint8_t interfaceId, Storage &storage, Storage &counters);

	~BusInterface() override;

	/**
	 * Set configuration. Interface starts on first call of this method
	 * @param key network key
	 * @param aesKey network key prepared for aes encryption
	 */
	void setConfiguration(DataBuffer<16> const &key, AesKey const &aesKey);

	String getName() override;
	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getElementIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	SubscriberTarget getSubscriberTarget(uint8_t id, uint8_t plugIndex) override;
	void subscribe(Subscriber &subscriber) override;
	void listen(Listener &listener) override;
	void erase(uint8_t id) override;

private:
	static constexpr int MAX_PLUG_COUNT = 64;
	static constexpr int MESSAGE_LENGTH = 80;


	// device data that is stored in flash
	struct DeviceData {
		// device id, also used as storage index
		uint8_t id;

		// bus address (is equal to id - 1)
		uint8_t address;

		// id of device on bus
		uint32_t busDeviceId;
	};

	class Endpoint;
	class Device {
	public:
		static constexpr int MESSAGE_LENGTH = 80;

		// constructor for loading devices on startup
		Device(BusInterface *interface, DeviceData const &data);

		// constructor for commissioning new devices
		Device(DeviceData const &data) : data(data), securityCounter(0) {}

		~Device();

		// next device
		Device *next;

		// device data that is stored in flash
		DeviceData data;

		// last security counter seen from bus device
		uint32_t securityCounter;

		// linked list of endpoints
		Endpoint *endpoints = nullptr;
	};

	struct EndpointData {
		uint8_t id;

		// id of device this endpoint belongs to (used while loading from flash)
		uint8_t deviceId;

		// device name, zero-terminated if shorter than maximum length
		char name[MAX_NAME_LENGTH];

		// number of plugs
		uint8_t plugCount;

		// plugs
		MessageType plugs[MAX_PLUG_COUNT];

		int size() {return offsetOf(EndpointData, plugs[this->plugCount]);}
	};

	class Endpoint : public Element {
	public:
		// adds to linked list and takes ownership of the data
		Endpoint(BusInterface *interface, Device *device, EndpointData *data)
			: Element(data->id, interface->listeners), next(nullptr), data(data)
		{
			// add to end of linked list to preserve endpoint index
			auto e = &device->endpoints;
			while (*e != nullptr)
				e = &(*e)->next;
			*e = this;
		}

		~Endpoint();

		// next endpoint in list
		Endpoint *next;

		// endpoint data that is stored in flash
		EndpointData *data;
	};

	Endpoint *getEndpoint(uint8_t id) const;
	uint8_t allocateId(int elementCount);
	uint8_t allocateDeviceId();
	Device *getOrLoadDevice(uint8_t deviceId);

	// start the interface
	//Coroutine start();


	// listeners that listen on all messages of the interface (as opposed to subscribers that subscribe to one plug)
	ListenerList listeners;

	// persistent storage
	Storage &storage;

	// persistent counters
	Storage &counters;
	uint32_t securityCounter;

	// configuration
	DataBuffer<16> const *key = nullptr;
	AesKey const *aesKey;

	// devices
	Device *devices = nullptr;
	int elementCount = 0;
	uint8_t elementIds[MAX_ELEMENT_COUNT];

	// receive from bus nodes
	Coroutine receive();

	[[nodiscard]] AwaitableCoroutine handleCommission(uint32_t busDeviceId, uint8_t endpointCount);

	[[nodiscard]] AwaitableCoroutine readAttribute(int &length, uint8_t (&message)[MESSAGE_LENGTH], Device &device,
		uint8_t endpointIndex, bus::Attribute attribute);

	// publish messages to bus nodes
	Coroutine publish();

	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	AwaitableCoroutine commissionCoroutine;
	Device *tempDevice;


	struct Response {
		// response data
		int& length;
		uint8_t *response;

		// address of device from which we expect a response
		uint8_t address;

		// endpoint from which we expect a response
		uint8_t endpointIndex;

		// attribute we are waiting for
		bus::Attribute attribute;
	};

	// a coroutine (e.g. handleZbCommission()) waits on this barrier until a response arrives
	Barrier<Response> responseBarrier;

	// publish() coroutine waits here until something gets published to a bus device
	SubscriberBarrier publishBarrier;
};
