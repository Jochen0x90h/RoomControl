#pragma once

#include "Message.hpp"
#include <LinkedList.hpp>
#include <functional>


// connection data stored in flash
struct Connection {
	struct {
		uint8_t interfaceId;
		uint8_t deviceId;
		uint8_t plugIndex;
	} source;

	struct {
		uint8_t plugIndex;
	} destination;

	// message convert options, depend on endpoint and message types
	ConvertOptions convertOptions;
};

/**
 * Info for the subscriber of a message
 */
struct SubscriberInfo {
	// connection index
	uint8_t connectionIndex;

	// receiver device id
	uint8_t deviceId;

	// receiver plug index
	uint8_t plugIndex;

	// message type
	//MessageType type;
};

struct SubscriberParameters {
public:
	// info about the message source for the subscriber to identify the message
	SubscriberInfo &info;

	// message (length is defined by the message type)
	void *message;
};

using SubscriberBarrier = Barrier<SubscriberParameters>;

struct SubscriberTarget {
	MessageType type = MessageType(0);
	SubscriberBarrier *barrier = nullptr;
};

/**
 * Subscriber
 */
class Subscriber : public LinkedListNode {
public:
	Connection const *data;

	uint8_t deviceId;
	uint8_t connectionIndex;

	SubscriberTarget target;
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

	void publishInt8(uint8_t plugIndex, int8_t value);

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



/**
 * Info for the listener of a message
 */
struct ListenerInfo {
	// interface index
	uint8_t interfaceId;

	// sender device id
	uint8_t deviceId;

	// sender plug index
	uint8_t plugIndex;

	// message type
	//MessageType type;
};

struct ListenerParameters {
public:
	// info about the message source for the subscriber to identify the message
	ListenerInfo &info;

	// message (length is defined by the message type)
	void *message;
};

using ListenerBarrier = Barrier<ListenerParameters>;

class Listener : public LinkedListNode {
public:
	//std::function<bool (uint8_t interfaceId, uint8_t deviceId, uint8_t plugIndex)> filter;

	ListenerBarrier *barrier;
};

class ListenerList : public LinkedList<Listener> {
public:
	ListenerList(uint8_t interfaceId) : interfaceId(interfaceId) {}

	/**
	 * Publish switch message to listeners
	 * @param deviceId device id
	 * @param plugIndex plug index
	 * @param value value of message to publish
	 */
	void publishSwitch(uint8_t deviceId, uint8_t plugIndex, uint8_t value);

	void publishInt8(uint8_t deviceId, uint8_t plugIndex, int8_t value);

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


	uint8_t interfaceId;
};
