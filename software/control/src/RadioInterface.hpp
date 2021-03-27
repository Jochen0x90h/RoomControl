#pragma once

#include <Interface.hpp>
#include <Storage.hpp>
#include <crypt.hpp>
#include <config.hpp>


/**
 * Devices connected by radio using IEEE 802.15.4 standard
 * Emulator implementation uses virtual devices on user interface
 */
class RadioInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;


	/**
	 * Constructor
	 * @param onSent gets called when send queue is empty, also initialy when all devices on the bus are enumerated
	 * @param onReceived gets called when data was reveived from an endpoint (endpointId, data, length)
	 */
	RadioInterface(Storage &storage, std::function<void (uint8_t, uint8_t const *, int)> const &onReceived);

	~RadioInterface() override;

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	DeviceId getDeviceId(int index) override;

	Array<EndpointType> getEndpoints(DeviceId deviceId) override;

	void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;
	void unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void send(uint8_t endpointId, uint8_t const *data, int length) override;

	//bool isBusy() {return this->busy;}

private:
	
	void onRx(uint8_t const *data);

	void commission(uint32_t deviceId, uint8_t const *data, int length);


	struct Device {
		uint32_t deviceId;
		AesKey aesKey;
		uint8_t deviceType;
	
		EndpointType endpointTypes[4];
	
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

	struct DeviceState{
		// security counter
		uint32_t counter;
		
		// endpoints
		uint8_t firstEndpointId;
		uint8_t referenceCounters[4];
	};

	// list of commissioned devices
	Storage::Array<Device, DeviceState> devices;

	std::function<void (uint8_t, uint8_t const *, int)> onReceived;

	// true for commissioning mode
	bool commissioning = false;

	// endpoint reference counters
	//uint8_t referenceCounters[MAX_ENDPOINT_COUNT];
	uint8_t nextEndpointId = 1;
};
