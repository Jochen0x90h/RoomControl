#pragma once

#include <Interface.hpp>
#include <Storage.hpp>
#include <crypt.hpp>
#include <config.hpp>


/**
 * Devices connected by radio using IEEE 802.15.4 standard
 */
class RadioInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;

	/**
	 * Constructor
	 * @param onSent gets called when send queue is empty, also initialy when all devices on the bus are enumerated
	 * @param onReceived gets called when data was reveived from an endpoint (endpointId, data, length)
	 */
	RadioInterface(std::function<void (uint8_t, uint8_t const *, int)> const &onReceived);

	~RadioInterface() override;

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	DeviceId getDeviceId(int index) override;

	Array<EndpointType> getEndpoints(DeviceId deviceId) override;

	void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;
	void unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void send(uint8_t endpointId, uint8_t const *data, int length) override;

	//bool isBusy() {return this->busy;}


	struct Coordinator {
		// long coordinator address (short address is always 0x0000)
		uint64_t longAddress;
		
		// pan
		uint16_t pan;
	
		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;
	};
	
	struct CoordinatorState {
	};

	struct Device {
		enum Type : uint8_t {
			ZIGBEE,
			GREEN_POWER
		};
		
		// device id, either ieee 802.15.4 long device address or 32 bit green power id
		DeviceId deviceId;

		// radio device type
		Type type;
		
		// device type from commissioning message
		uint8_t deviceType;
	
		// endpoints derived from device type (zero terminated list)
		EndpointType endpointTypes[4];

		union {
			// short device address
			uint16_t shortAddress;
			
			// device key for green power devices
			AesKey key;
		} u;
		
		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;
		
		/**
		 * Returns the number of endpoints
		 * @return number of endpoints
		 */
		int getEndpointCount() const;
	};

	struct DeviceState {
		// security counter
		uint32_t counter;
		
		// endpoint subscriptions
		uint8_t firstEndpointId;
		uint8_t referenceCounters[4];
		
		uint8_t state;
	};


	// coordinator (array of length 1)
	Storage::Array<Coordinator, CoordinatorState> coordinator;

	// list of commissioned devices
	Storage::Array<Device, DeviceState> devices;

private:

	void onRx(uint8_t *data, int length);

	void commission(uint32_t deviceId, uint8_t *d, uint8_t const *end);


	// radio context id
	uint8_t radioId;

	std::function<void (uint8_t, uint8_t const *, int)> onReceived;

	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	// endpoint reference counters
	uint8_t nextEndpointId = 1;
};
