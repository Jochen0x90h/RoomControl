#pragma once

#include "Message.hpp"
#include <Coroutine.hpp>
#include <LinkedListNode.hpp>


/*
struct Subscriber : public LinkedListNode<Subscriber>, public PublishInfo {
	// message source, set by subscribe() method
	// todo: type not necessary?
	MessageInfo source;

	// message convert options, depend on endpoint and message types
	ConvertOptions convertOptions;
};
*/


// connection data stored in flash
struct ConnectionData {

	struct PlugInfo {
		uint8_t interfaceIndex;
		uint8_t deviceId;
		uint8_t plugIndex;
	};

	PlugInfo source;
	PlugInfo destination;

	// message convert options, depend on endpoint and message types
	ConvertOptions convertOptions;
};


struct SubscriberInfo {
	MessageType type = MessageType(0);

	struct Parameters {
		// info about the message source for the subscriber to identify the message
		MessageInfo &info;

		// message (length is defined by Subscriber::messageType)
		void *message;
	};

	using Barrier = ::Barrier<Parameters>;
	Barrier *barrier = nullptr;
};


class Subscriber : public LinkedListNode<Subscriber> {
public:
	ConnectionData const *data;
	uint8_t connectionIndex;

	SubscriberInfo info;
};


/**
 * List of subscribers with methods to publish messages to the subscribers
 */
class SubscriberList : public LinkedListNode<Subscriber> {
public:
	/**
	 * Publish switch message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param message message to publish
	 */
	void publishSwitch(int plugIndex, uint8_t message);

	/**
	 * Publish float message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param message message to publish
	 */
	void publishFloat(int plugIndex, float message);

	/**
	 * Publish float message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param message message to publish
	 * @param command command: 0 = set, 1 = step
	 */
	void publishFloatCommand(int plugIndex, float message, uint8_t command);

	/**
	 * Publish float message to subscribers to given plug
	 * @param plugIndex plug index
	 * @param message message to publish
	 * @param command command: 0 = set, 1 = step
	 * @param transition transition in 1/10s
	 */
	void publishFloatTransition(int plugIndex, float message, uint8_t command, uint16_t transition);
};
