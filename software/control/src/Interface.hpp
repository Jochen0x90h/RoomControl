#pragma once

#include "Array.hpp"
#include "Device.hpp"
#include <functional>


/**
 * Interface to devices connected locally, via a bus or radio
 */
class Interface {
public:

	/**
	 * Destructor
	 */
	virtual ~Interface();
	
	/**
	 * Get list of devices connected to this interface
	 * @return list of device ids
	 */
	virtual Array<DeviceId> getDevices() = 0;
	
	/**
	 * Get list of endpoints for a device
	 * @param deviceId id device
	 * @return list of endpoints
	 */
	virtual Array<EndpointType> getEndpoints(DeviceId deviceId) = 0;

	/**
	 * Subscribe to an endpoint on the bus
	 * @param receives the endpoint id. A new reference is counted if it was zero, otherwise it must be the same id
	 * @param deviceId device id
	 * @param endpointIndex endpoint index of device
	 */
	virtual void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) = 0;

	/**
	 * Unsubscribe from an endpoint on the bus
	 * @param endpointId endpoint id, gets set to zero and the reference counter is decremented
	 */
	virtual void unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) = 0;

	/**
	 * Get number of transfers in progress
	 * @return number of transfers in progress
	 */
	//int getSendCount() {return this->transferCount;}

	/**
	 * Send a message to a device. Can be called multiple times in a row. When all messages are sent, onSent gets called
	 * @param endpointId receiver of the message
	 * @param data message data
	 * @param length message length, maximum is 8
	 */
	virtual void send(uint8_t endpointId, uint8_t const *data, int length) = 0;
	
};
