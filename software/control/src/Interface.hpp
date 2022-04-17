#pragma once

#include "Device.hpp"
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

	using EndpointType = bus::EndpointType;
	
	/**
	 * Destructor
	 */
	virtual ~Interface();
	
	/**
	 * Set the interface into commissioning mode so that new devices can be added
	 * @param enabled true to enable
	 */
	virtual void setCommissioning(bool enabled) = 0;


	class Device {
	public:
		/**
		 * Get device id
		 * @return device id
		 */
		virtual DeviceId getId() = 0;

		/**
		 * Get device name
		 * @return device name
		 */
		virtual String getName() = 0;

		/**
		 * Set device name
		 * @param device name
		 */
		virtual void setName(String name) = 0;

		/**
		 * Get endpoints
		 * @return endpoints
		 */
		virtual Array<EndpointType const> getEndpoints() = 0;

		/**
		 * Add a publisher to the device that sends messages to an endpoint. Gets inserted into a linked list
		 * @param endpointIndex endpoint index
		 * @param publisher publisher to insert
		 */
		virtual void addPublisher(uint8_t endpointIndex, Publisher &publisher) = 0;

		/**
		 * Add a subscriber to the device that receives messages from an endpoint. Gets inserted into a linked list
		 * @param endpointIndex endpoint index
		 * @param publisher subscriber to insert
		 */
		virtual void addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) = 0;
	};

	/**
	 * Get number of devices connected to this interface
	 */
	virtual int getDeviceCount() = 0;

	/**
	 * Get a device by index
	 * @param index index of device
	 * @return device
	 */
	virtual Device &getDeviceByIndex(int index) = 0;

	/**
	 * Get a device by id
	 * @param id device id
	 * @return device
	 */
	virtual Device *getDeviceById(DeviceId id) = 0;
};
