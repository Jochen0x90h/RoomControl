#pragma once

#include "Interface.hpp"
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <State.hpp>
#include <appConfig.hpp>


/**
 * Devices connected by bus using LIN physical layer
 */
class BusInterface : public Interface {
public:
	// maximum number of devices that can be commissioned (each endpoint counts as one device)
	static constexpr int MAX_DEVICE_COUNT = 128;

	// number of coroutines for publishing
	static constexpr int PUBLISH_COUNT = 4;

	/**
	 * Constructor
	 * @param configuration global configuration
	 * @param stateManager persistent state manager for counters
	 */
	BusInterface(PersistentStateManager &stateManager);

	~BusInterface() override;

	/**
	 * Set configuration. Interface starts on first call of this method
	 * @param key network key
	 * @param aesKey network key prepared for aes encryption
	 */
	void setConfiguration(DataBuffer<16> const &key, AesKey const &aesKey);

	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getDeviceIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	void subscribe(uint8_t id, uint8_t plugIndex, Subscriber &subscriber) override;
	PublishInfo getPublishInfo(uint8_t id, uint8_t plugIndex) override;
	void erase(uint8_t id) override;

private:
	static constexpr int MAX_PLUG_COUNT = 64;
	static constexpr int MESSAGE_LENGTH = 80;


	// device data that is stored in flash
	struct DeviceData {
		// storage id, also used as bus address
		uint8_t id;

		// device id
		uint32_t deviceId;
	};

	class Endpoint;
	class BusDevice {
	public:
		static constexpr int MESSAGE_LENGTH = 80;

		BusDevice(DeviceData const &data)
			: data(data)
		{}
		BusDevice(BusInterface *interface, DeviceData const &data)
			: next(interface->devices), data(data)
		{
			interface->devices = this;
		}
		~BusDevice();

		// next device
		BusDevice *next;

		// device data that is stored in flash
		DeviceData data;

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

	class Endpoint {
	public:
		// takes ownership of the data
		Endpoint(BusDevice *device, EndpointData *data)
			: next(nullptr), data(data)
		{
			// add new endpoint at end of linked list of device
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

		// list of subscribers
		SubscriberList subscribers;
	};

	Endpoint *getEndpoint(uint8_t id) const;
	uint8_t allocateId(int deviceCount);
	uint8_t allocateDeviceId();
	BusDevice *getOrLoadDevice(uint8_t id);

	// start the interface
	Coroutine start();

	// counters
	PersistentState<uint32_t> securityCounter;

	// configuration
	DataBuffer<16> const *key = nullptr;
	AesKey const *aesKey;


	int deviceCount = 0;
	uint8_t deviceIds[MAX_DEVICE_COUNT];
	BusDevice *devices = nullptr;

	// receive from bus nodes
	Coroutine receive();

	[[nodiscard]] AwaitableCoroutine handleCommission(uint32_t deviceId, uint8_t endpointCount);

	[[nodiscard]] AwaitableCoroutine readAttribute(int &length, uint8_t (&message)[MESSAGE_LENGTH], BusDevice &device,
		uint8_t endpointIndex, bus::Attribute attribute);

	// publish messages to bus nodes
	Coroutine publish();

	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	AwaitableCoroutine commissionCoroutine;
	BusDevice *tempDevice;


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
	Waitlist<Response> responseBarrier;

	PublishInfo::Barrier publishBarrier;
};
