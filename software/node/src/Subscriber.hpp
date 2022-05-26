#pragma once

#include <LinkedListNode.hpp>


struct Subscriber : public LinkedListNode<Subscriber> {
	// endpoint or topic index, set by interface or broker
	uint16_t index;

	// info about the message source
	struct Source {
		// plug index, set by subscriber to identify the plug over which the message came
		uint8_t plugIndex;

		// connection index, set by subscriber to identify the connection to the plug over which the message came
		uint8_t connectionIndex;
	};

	// connection information, set by subscriber to identify the message
	Source source;

	// the message type that the subscriber expects (broker is responsible for conversion)
	MessageType messageType = MessageType::UNKNOWN;

	// message convert options, depend on endpoint and message types
	ConvertOptions convertOptions;

	struct Parameters {
		// info about the message source for the subscriber to identify the message
		Source &source;
				
		// message (length is defined by Subscriber::messageType)
		void *message;
	};

	// barrier where the subscriber waits for new messages (owned by subscriber)
	using Barrier = ::Barrier<Parameters>;
	Barrier *barrier = nullptr;
};
using SubscriberList = LinkedListNode<Subscriber>;
