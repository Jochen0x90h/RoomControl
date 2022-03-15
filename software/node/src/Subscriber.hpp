#pragma once

#include <LinkedListNode.hpp>


struct Subscriber : public LinkedListNode<Subscriber> {
	// endpoint or topic index, set by interface or broker
	uint16_t index;
	
	// index of subscription, set by subscriber to identify the message
	uint8_t subscriptionIndex;

	// the message type that the subscriber expects (broker is responsible for conversion)
	MessageType messageType;
	
	struct Parameters {
		// index of subscription (needed by subscriber to identify the message)
		uint8_t &subscriptionIndex;
				
		// message (length is defined by Subscriber::messageType)
		void *message;
	};

	// barrier where the subscriber waits for new messages (owned by subscriber)
	using Barrier = ::Barrier<Parameters>;
	Barrier *barrier = nullptr;
};
using SubscriberList = LinkedListNode<Subscriber>;
