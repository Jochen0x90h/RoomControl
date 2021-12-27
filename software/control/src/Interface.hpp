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
	 * Get list of endpoints for a device
	 * @param deviceId device id
	 * @return list of endpoints
	 */
	virtual Array<EndpointType const> getEndpoints(DeviceId deviceId) = 0;

/*
	struct Publisher : public LinkedListNode<Publisher> {
		// index of device endpoint
		uint8_t endpointIndex;

		// type of message to be published
		MessageType messageType;

		// message (length is defined by messageType)
		void const *message;

		// indicates if the data has changed and needs to be published
		bool dirty = false;
		
		// event to notify the interface that the dirty flag was set (set and owned by interface)
		Event *event = nullptr;
		
		void publish() {
			this->dirty = true;
			if (this->event != nullptr)
				this->event->set();
		}
	};
	using PublisherList = LinkedListNode<Publisher>;
*/
	/**
	 * Add a publisher to the device. Gets inserted into a linked list
	 * @param deviceId device id
	 * @param publisher publisher to insert
	 */
	virtual void addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) = 0;

/*
	struct Subscriber : public LinkedListNode<Subscriber> {
		// index of device endpoint
		uint8_t endpointIndex;
		
		// index of subscription, set by subscriber to identify the message
		uint8_t subscriptionIndex;

		// the message type that the subscriber expects (interface is responsible for conversion)
		MessageType messageType;
		
		struct Parameters {
			// index of subscription (needed by subscriber to identify the message)
			uint8_t &subscriptionIndex;
					
			// message (length is defined by Subscriber::messageType)
			void *message;
		};

		// barrier where the subscriber waits for new messages (owned by subscriber)
		using Barrier = Barrier<Parameters>;
		Barrier *barrier = nullptr;
	};
	using SubscriberList = LinkedListNode<Subscriber>;
*/

	/**
	 * Add a subscriber to the device. Gets inserted into a linked list
	 * @param deviceId device id
	 * @param publisher subscriber to insert
	 */
	virtual void addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) = 0;
};
