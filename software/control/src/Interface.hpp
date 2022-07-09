#pragma once

#include <bus.hpp>
#include <Publisher.hpp>
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


	class Device {
	public:
		virtual ~Device();

		/**
		 * Get device id which is stable when other devices are added or removed
		 * @return device id
		 */
		virtual uint8_t getId() const = 0;

		/**
		 * Get device name
		 * @return device name
		 */
		virtual String getName() const = 0;

		/**
		 * Set device name
		 * @param device name
		 */
		virtual void setName(String name) = 0;

		/**
		 * Get endpoints (message type for each endpoint)
		 * @return array of endpoints
		 */
		virtual Array<MessageType const> getPlugs() const = 0;

		/**
		 * Subscribe to receive messages messages from an endpoint
		 * @param endpointIndex endpoint index
		 * @param subscriber subscriber to insert, gets internally inserted into a linked list
		 */
		virtual void subscribe(uint8_t endpointIndex, Subscriber &subscriber) = 0;

		/**
		 * Get publish info used to publish a message to an endpoint
		 * @param endpointIndex
		 * @return publish info
		 */
		virtual PublishInfo getPublishInfo(uint8_t endpointIndex) = 0;
	};

	/**
	 * Get list of devices
	 * @return list of device id's
	 */
	//virtual Array<uint8_t const> getDevices() = 0;

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
	virtual Device *getDevice(uint8_t id) = 0;

	/**
	 * Erase a device by id
	 * @param id device id
	 */
	virtual void eraseDevice(uint8_t id) = 0;


	// helper function: allocate a free interface id
	template <typename T>
	static uint8_t allocateInterfaceId(T const &devices) {
		// find a free id
		int id;
		for (id = 1; id < 256; ++id) {
			for (auto &device : devices) {
				if (device->interfaceId == id)
					goto found;
			}
			break;
		found:
			;
		}
		return id;
	}
};
