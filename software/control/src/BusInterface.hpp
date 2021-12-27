#pragma once

#include "Interface.hpp"
#include "Storage.hpp"
#include <Configuration.hpp>
#include <DataReader.hpp>
#include <DataWriter.hpp>
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
	BusInterface(Configuration &configuration, PersistentStateManager &stateManager);

	~BusInterface() override;

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;

	DeviceId getDeviceId(int index) override;

	Array<EndpointType const> getEndpoints(DeviceId deviceId) override;

	void addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) override;

	void addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) override;

	class MessageReader : public DecryptReader {
	public:
		MessageReader(int length, uint8_t *data) : DecryptReader(length, data) {}
		
		/**
		 * Read a value from 0 to 8 from bus arbitration, i.e. multiple devices can write at the same time and the
		 * lowest value survives
		 */
		uint8_t arbiter();
	};
	
	class MessageWriter : public EncryptWriter {
	public:
		MessageWriter(uint8_t *message) : EncryptWriter(message), begin(message) {}

		/**
		 * Write a value from 0 to 8 for bus arbitration, i.e. multiple devices can write at the same time and the
		 * lowest value survives
		 */
		void arbiter(uint8_t value);

		int getLength() {
			return this->current - this->begin;
		}

		uint8_t *begin;
	};

private:

	// reference to global configuration (array contains only one element)
	Configuration &configuration;

	// initialize the interface
	Coroutine init(PersistentStateManager &stateManager);

	AwaitableCoroutine commission();
	
	AwaitableCoroutine commissionCoroutine;


	// devices
	struct Device;
	
	struct DeviceFlash {
		static constexpr int MAX_ENDPOINT_COUNT = 32;

		// device id
		uint32_t deviceId;

		uint8_t address;

		// number of endpoints of the device
		uint8_t endpointCount;
				
		// endpoint types
		EndpointType endpoints[MAX_ENDPOINT_COUNT];
	
		// note: endpoints must be the last member
	
		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int size() const;

		/**
		 * Allocates a state object
		 * @return new state object
		 */
		Device *allocate() const;
	};

	class Device : public Storage::Element<DeviceFlash> {
	public:
		Device(DeviceFlash const &flash) : Storage::Element<DeviceFlash>(flash) {}
		
		SubscriberList subscribers;
		PublisherList publishers;
	};

public:
	// list of commissioned devices
	Storage::Array<Device> devices;
	
private:
	bool receive(MessageReader &r);

	// wait for a request by a device
	Coroutine awaitRequest();

	// publish messages to devices
	Coroutine publish();

	PersistentState<uint32_t> securityCounter;
	Event publishEvent;
};
