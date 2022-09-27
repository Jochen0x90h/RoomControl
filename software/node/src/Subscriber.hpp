#pragma once

#include "Message.hpp"
#include <LinkedList.hpp>


// connection data stored in flash
struct ConnectionData {
	struct {
		uint8_t interfaceIndex;
		uint8_t deviceId;
		uint8_t plugIndex;
	} source;

	struct {
		uint8_t plugIndex;
	} destination;

	// message convert options, depend on endpoint and message types
	ConvertOptions convertOptions;
};


struct SubscriberInfo {
	MessageType type = MessageType(0);
	MessageBarrier *barrier = nullptr;
};

/**
 * Subscriber
 */
class Subscriber : public LinkedListNode {
public:
	ConnectionData const *data;

	uint8_t deviceId;
	uint8_t connectionIndex;

	SubscriberInfo info;
};


/**
 * List of subscribers with methods to publish messages to the subscribers
 */
class SubscriberList : public LinkedList<Subscriber> {
public:
	/**
	 * Publish switch message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 */
	void publishSwitch(uint8_t plugIndex, uint8_t value);

	/**
	 * Publish float message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 */
	void publishFloat(uint8_t plugIndex, float value);

	/**
	 * Publish float message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 * @param command command: 0 = set, 1 = step
	 */
	void publishFloatCommand(uint8_t plugIndex, float value, uint8_t command);

	/**
	 * Publish float message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 * @param command command: 0 = set, 1 = step
	 * @param transition transition in 1/10s
	 */
	void publishFloatTransition(uint8_t plugIndex, float value, uint8_t command, uint16_t transition);
};



class Listener : public LinkedListNode {
public:
	uint8_t sourceIndex;
	MessageBarrier *barrier;
};

class ListenerList : public LinkedList<Listener> {
public:
	/**
	 * Publish switch message to listeners
	 * @param deviceId device id
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 */
	void publishSwitch(uint8_t deviceId, uint8_t plugIndex, uint8_t value);

	/**
	 * Publish float message to listeners
	 * @param deviceId device id
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 */
	void publishFloat(uint8_t deviceId, uint8_t plugIndex, float value);

	/**
	 * Publish float message to subscribers to given plug
	 * @param deviceId device id
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 * @param command command: 0 = set, 1 = step
	 */
	void publishFloatCommand(uint8_t deviceId, uint8_t plugIndex, float value, uint8_t command);

	/**
	 * Publish float message to subscribers to given plug
	 * @param deviceId device id
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 * @param command command: 0 = set, 1 = step
	 * @param transition transition in 1/10s
	 */
	void publishFloatTransition(uint8_t deviceId, uint8_t plugIndex, float value, uint8_t command, uint16_t transition);
};
