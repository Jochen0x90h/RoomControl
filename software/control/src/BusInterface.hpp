#pragma once

#include <Interface.hpp>
#include <Array.hpp>
#include <Queue.hpp>
#include <config.hpp>


/**
 * Bus for connecting devices using wires
 * Emulator implementation uses virtual devices on user interface
 */
class BusInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;
	static constexpr int MAX_ENDPOINT_COUNT = 128;
	static constexpr int MAX_MESSAGE_SIZE = 8;

	static constexpr int MAX_SEND_COUNT = 32;
	static constexpr int SEND_BUFFER_SIZE = 128;


	/**
	 * Constructor
	 * @param onSent gets called when send queue is empty, also initialy when all devices on the bus are enumerated
	 * @param onReceived gets called when data was reveived from an endpoint (endpointId, data, length)
	 */
	BusInterface(std::function<void ()> const &onSent,
		std::function<void (uint8_t, uint8_t const *, int)> const &onReceived);

	~BusInterface() override;
	
	/**
	 * Get list of devices connected to the bus. Is not valid before onReady() gets called
	 * @return list of device ids
	 */
	Array<DeviceId> getDevices() override;
	
	Array<EndpointType> getEndpoints(DeviceId deviceId) override;

	void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void send(uint8_t endpointId, uint8_t const *data, int length) override;

	bool isBusy() {return this->busy;}

	static uint8_t calcChecksum(const uint8_t *data, int length);

private:
	
	void enumerate();
	void onEnumerated(int rxLength);
	void onTransferred(int rxLength);
	
	// called when an endpoint wants to be read
	void onRequest(uint8_t endpointId);
	
	std::function<void ()> onSent;
	std::function<void (uint8_t, uint8_t const *, int)> onReceived;

	// enumeration
	DeviceId deviceIds[MAX_DEVICE_COUNT];
	int deviceCount = 0;
	uint8_t endpointTypes[MAX_ENDPOINT_COUNT + 5 + 1];
	uint8_t endpointStarts[MAX_DEVICE_COUNT + 1];
	uint8_t retryCount = 0;
	
	// endpoints
	uint8_t referenceCounters[MAX_ENDPOINT_COUNT];
	
	// message send/receive
	Queue<bool, MAX_SEND_COUNT, SEND_BUFFER_SIZE> txQueue;
	bool busy = false;
	uint8_t rxData[1 + MAX_MESSAGE_SIZE + 1]; // endpoint id, message, checksum
};
