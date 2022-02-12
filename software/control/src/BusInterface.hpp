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
		template <int N>
		MessageWriter(uint8_t (&message)[N]) : EncryptWriter(message), begin(message)
#ifdef EMU
			, end(message + N)
#endif
		{}

		MessageWriter(int length, uint8_t *message) : EncryptWriter(message), begin(message)
#ifdef EMU
			, end(message + length)
#endif
		{}

		/**
		 * Write a value from 0 to 8 for bus arbitration, i.e. multiple devices can write at the same time and the
		 * lowest value survives
		 */
		void arbiter(uint8_t value);

		int getLength() {
			int length = this->current - this->begin;
#ifdef EMU
			assert(this->current <= this->end);
#endif
			return length;
		}

		uint8_t *begin;
#ifdef EMU
		uint8_t *end;
#endif
	};

private:

	// counters
	PersistentState<uint32_t> securityCounter;

	// configuration
	DataBuffer<16> const *key;
	AesKey const *aesKey;


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

	Event publishEvent;
};
