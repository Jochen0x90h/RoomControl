#pragma once

#include "Interface.hpp"
#include "Storage.hpp"
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <State.hpp>
#include <appConfig.hpp>


/**
 * Devices connected by bus using LIN physical layer
 */
class BusInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;

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

	int getDeviceCount() override;
	Device &getDeviceByIndex(int index) override;
	Device *getDeviceById(uint8_t id) override;

private:

	// start the interface
	Coroutine start();

	// counters
	PersistentState<uint32_t> securityCounter;

	// configuration
	DataBuffer<16> const *key = nullptr;
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
		MessageType2 endpoints[MAX_ENDPOINT_COUNT];

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
		~BusDevice() override;

		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<MessageType2 const> getEndpoints() const override;
		void subscribe(uint8_t endpointIndex, Subscriber &subscriber) override;
		PublishInfo getPublishInfo(uint8_t endpointIndex) override;

		// back pointer to interface
		BusInterface *interface = nullptr;

		// subscribers and publishers
		SubscriberList subscribers;
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

	PublishInfo::Barrier publishBarrier;
};
