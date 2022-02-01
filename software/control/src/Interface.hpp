#pragma once

#include "Device.hpp"
#include <Message.hpp>
#include <Publisher.hpp>
#include <State.hpp>
#include <Subscriber.hpp>
#include <Array.hpp>
#include <Coroutine.hpp>


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
	 * Set the interface into commissioning mode so that new devices can be added
	 * @param enabled true to enable
	 */
	virtual void setCommissioning(bool enabled) = 0;
	
	/**
	 * Get number of devices connected to this interface
	 */
	virtual int getDeviceCount() = 0;
	
	/**
	 * Get device id by index
	 * @param index index of device
	 * @return device id
	 */
	virtual DeviceId getDeviceId(int index) = 0;
	
	/**
	 * Remove the device from the internal list of devices. It the has to be commissioned again
	 */
	//virtual void removeDevice() = 0;
	
	/**
	 * Get list of endpoints for a device
	 * @param deviceId device id
	 * @return list of endpoints
	 */
	virtual Array<EndpointType const> getEndpoints(DeviceId deviceId) = 0;

	/**
	 * Add a publisher to the device. Gets inserted into a linked list
	 * @param deviceId device id
	 * @param publisher publisher to insert
	 */
	virtual void addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) = 0;

	/**
	 * Add a subscriber to the device. Gets inserted into a linked list
	 * @param deviceId device id
	 * @param publisher subscriber to insert
	 */
	virtual void addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) = 0;
};
