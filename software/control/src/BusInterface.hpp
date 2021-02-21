#pragma once

#include <Interface.hpp>
#include <Array.hpp>
#include <config.hpp>


/**
 * Bus for connecting devices using wires
 * Emulator implementation uses virtual devices on user interface
 */
class BusInterface : public Interface {
public:

	/**
	 * Constructor
	 * @param onReady gets called when all devices on the bus are enumerated
	 */
	BusInterface(std::function<void ()> onReady);

	~BusInterface() override;
	
	/**
	 * Get list of devices connected to the bus. Is not valid before onReady() gets called
	 * @return list of device ids
	 */
	Array<DeviceId> getDevices() override;
	
	Array<EndpointType> getEndpoints(DeviceId deviceId) override;

	void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void unsubscribe(uint8_t &endpointId) override;

	bool send(uint8_t endpointId, uint8_t const *data, int length/*, std::function<void ()> onSent*/) override;
	
	void setReceiveHandler(std::function<void (uint8_t, uint8_t const *, int)> onReceived) override;
	
	static uint8_t calcChecksum(const uint8_t *data, int length);

private:
	
	void enumerate();
	void onEnumerated(int rxLength);
	
	// called when an endpoint wants to be read
	void onRequest(uint8_t endpointId);
	
	std::function<void ()> onReady;
	
	DeviceId deviceIds[MAX_DEVICE_COUNT];
	int deviceCount = 0;
	uint8_t endpointTypes[128 + 5 + 1];
	uint8_t endpointStarts[MAX_DEVICE_COUNT + 1];
	uint8_t retryCount = 0;
	
	// reference counter for each endpoint id
	uint8_t referenceCounters[128];
	
	std::function<void (uint8_t, uint8_t const *, int)> onReceived;
	uint8_t rxData[10];
	
	uint8_t txData[128];
	
	
};
