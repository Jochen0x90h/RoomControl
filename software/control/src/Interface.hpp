#pragma once

#include <bus.hpp>
#include <Subscriber.hpp>
#include <Array.hpp>
#include <Coroutine.hpp>


/**
 * Interface to devices connected locally, via a bus or radio
 */
class Interface {
public:

	static constexpr int MAX_NAME_LENGTH = 16;

	/**
	 * Destructor
	 */
	virtual ~Interface();

	/**
	 * Get name of interface
	 */
	virtual String getName() = 0;

	/**
	 * Set the interface into commissioning mode so that new devices can be added
	 * @param enabled true to enable
	 */
	virtual void setCommissioning(bool enabled) = 0;

	/**
	 * Get list of device id's
	 * @return list of device id's
	 */
	virtual Array<uint8_t const> getDeviceIds() = 0;

	/**
	 * Get device name
	 * @return device name
	 */
	virtual String getName(uint8_t id) const = 0;

	/**
	 * Set device name
	 * @param device name
	 */
	virtual void setName(uint8_t id, String name) = 0;

	/**
	 * Get plugs (message type for each plug). Use immediately and don't use after a co_await
	 * @return array of plugs
	 */
	virtual Array<MessageType const> getPlugs(uint8_t id) const = 0;

	/**
	 * Get information necessary to subscribe this plug to another plug using subscribe()
	 * @param id device id
	 * @param plugIndex plug index
	 * @return SubscriberTarget
	 */
	virtual SubscriberTarget getSubscriberTarget(uint8_t id, uint8_t plugIndex) = 0;

	/**
	 * Subscribe to receive messages messages from an endpoint
	 * @param subscriber subscriber to insert, gets internally inserted into a linked list
	 */
	virtual void subscribe(Subscriber &subscriber) = 0;

	/**
	 * Listen on all messages of the whole interface
	 * @param listener listener to insert, gets internally inserted into a linked list
	 */
	virtual void listen(Listener &listener) = 0;

	/**
	 * Erase a device by id. Make sure that all PublishInfo's obtained with getPublishInfo() are erased too
	 * @param id device id
	 */
	virtual void erase(uint8_t id) = 0;

protected:

	static int eraseId(int deviceCount, uint8_t *deviceIds, uint8_t id) {
		int j = 0;
		for (int i = 0; i < deviceCount; ++i) {
			uint8_t id2 = deviceIds[i];
			if (id2 != id) {
				deviceIds[j] = id2;
				++j;
			}
		}
		return j;
	}

	class Device {
	public:
		Device(uint8_t id, ListenerList &listeners) : id(id), listeners(listeners) {}

		void publishSwitch(uint8_t plugIndex, uint8_t value) {
			this->listeners.publishSwitch(this->id, plugIndex, value);
			this->subscribers.publishSwitch(plugIndex, value);
		}

		void publishFloat(uint8_t plugIndex, float value) {
			this->listeners.publishFloat(this->id, plugIndex, value);
			this->subscribers.publishFloat(plugIndex, value);
		}

		void publishFloatCommand(uint8_t plugIndex, float value, uint8_t command) {
			this->listeners.publishFloatCommand(this->id, plugIndex, value, command);
			this->subscribers.publishFloatCommand(plugIndex, value, command);
		}

		void publishFloatTransition(uint8_t plugIndex, float value, uint8_t command, uint16_t transition) {
			this->listeners.publishFloatTransition(this->id, plugIndex, value, command, transition);
			this->subscribers.publishFloatTransition(plugIndex, value, command, transition);
		}

		uint8_t id;

		// list of listeners (listen to all messages)
		ListenerList &listeners;

		// list of subscribers (subscribe for messages on a plug)
		SubscriberList subscribers;
	};
};
