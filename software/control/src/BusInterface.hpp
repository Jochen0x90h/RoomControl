#pragma once

#include <Interface.hpp>
#include <DataQueue.hpp>
#include <config.hpp>


/**
 * Bus for connecting devices using wires
 *
 * Protocol is based on LIN physical layer and uses 19200 baud
 * Frame format:
 * - BREAK, SYNC, <1 byte endpoint id>, <payload>, <8 bit crc>
 * - todo: BREAK, SYNC, <1 byte endpoint id>, <1 byte security counter>, <encrypted payload>, <2 byte authentication>
 * - First three components always sent by master, remaining components sent by endpoint in master or slave
 * Manual Commissioning:
 * - When a slave wants to be commissioned, it sends endpoint id 0 as a request
 * - Continue with procedure of auto commissioning when the master is in commissioning mode
 * Auto Commissioning:
 * - Master sends endpoint id 0, slaves reply with their device id followed by a list of endpoint types. A CAN-style
 *   arbitration ensures that only one slave "survives", all other slaves retreat.
 * - Master assigns an endpoint to the device by sending endpoint id 1 followed by the device id and the assigned
 *   device endpoint id
 * Endpoint subscription:
 * - Master sends endpoint id of device, the endpoint index to subscribe and the new endpoint id
 * Endpoint unsubscription
 * - Master sends endpoint id of device, the endpoint index to unsubscribe and 0 as new endpoint id
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

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	DeviceId getDeviceId(int index) override;

	/**
	 * Get list of devices connected to the bus. Is not valid before onReady() gets called
	 * @return list of device ids
	 */
	//Array<DeviceId> getDevices() override;
	
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

	uint8_t retryCount = 0;

	// enumeration
	DeviceId deviceIds[MAX_DEVICE_COUNT];
	int deviceCount = 0;
	uint8_t endpointTypes[MAX_ENDPOINT_COUNT + 5 + 1];
	uint8_t endpointStarts[MAX_DEVICE_COUNT + 1];
	
	// endpoint refrence counters
	uint8_t referenceCounters[MAX_ENDPOINT_COUNT];
	
	// transmit qeueue
	DataQueue<bool, MAX_SEND_COUNT, SEND_BUFFER_SIZE> txQueue;
	bool busy = false;
	
	// receive data
	uint8_t rxData[1 + MAX_MESSAGE_SIZE + 1]; // endpoint id, message, checksum
};
