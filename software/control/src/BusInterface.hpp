#pragma once

#include "Interface.hpp"
#include "Storage.hpp"
#include <Configuration.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <appConfig.hpp>


/**
 * Devices connected by bus using LIN physical layer
 */
class BusInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;

	/**
	 * Constructor
	 * @param configuration global configuration
	 * @param stateManager persistent state manager for counters
	 */
	BusInterface(PersistentStateManager &stateManager);

	~BusInterface() override;

	// start the interface
	Coroutine start(DataBuffer<16> const &key, AesKey const &aesKey);

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	Device &getDeviceByIndex(int index) override;
	Device *getDeviceById(uint8_t id) override;

private:

	// counters
	PersistentState<uint32_t> securityCounter;

	// configuration
	DataBuffer<16> const *key;
	AesKey const *aesKey;


	// devices
	class BusDevice;

	struct DeviceFlash {
		static constexpr int MAX_ENDPOINT_COUNT = 32;

		// device id
		uint32_t deviceId;

		// interface id
		uint8_t interfaceId;

		// device address
		uint8_t address;

		// number of endpoints of the device
		uint8_t endpointCount;

		// endpoint types
		EndpointType endpoints[MAX_ENDPOINT_COUNT];

		// note: endpoints must be the last member because of variable size allocation

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int size() const;

		/**
		 * Allocates a state object
		 * @return new state object
		 */
		BusDevice *allocate() const;
	};

	class BusDevice : public Storage::Element<DeviceFlash>, public Device {
	public:
		explicit BusDevice(DeviceFlash const &flash) : Storage::Element<DeviceFlash>(flash) {}

		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<EndpointType const> getEndpoints() const override;
		void addPublisher(uint8_t endpointIndex, Publisher &publisher) override;
		void addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) override;

		// back pointer to interface
		BusInterface *interface;

		// subscribers and publishers
		SubscriberList subscribers;
		PublisherList publishers;
	};

public:
	// list of commissioned devices
	Storage::Array<BusDevice> devices;

private:
	// receive from bus nodes
	Coroutine receive();

	// publish messages to bus nodes
	Coroutine publish();

	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	Event publishEvent;
};
