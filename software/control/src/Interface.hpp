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

	//using EndpointType = bus::EndpointType;
	
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
		virtual Array<MessageType const> getEndpoints() const = 0;

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
	virtual Device *getDeviceById(uint8_t id) = 0;


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
