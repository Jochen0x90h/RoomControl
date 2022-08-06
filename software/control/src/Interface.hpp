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

	static constexpr int MAX_NAME_LENGTH = 16;

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
	 * Subscribe to receive messages messages from an endpoint
	 * @param plugIndex plug index
	 * @param subscriber subscriber to insert, gets internally inserted into a linked list
	 */
	virtual void subscribe(uint8_t id, uint8_t plugIndex, Subscriber &subscriber) = 0;

	/**
	 * Get publish info used to publish a message to an endpoint
	 * @param plugIndex plug index
	 * @return publish info
	 */
	virtual PublishInfo getPublishInfo(uint8_t id, uint8_t plugIndex) = 0;

	/**
	 * Erase a device by id
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
};
