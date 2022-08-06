#pragma once

#include "Publisher.hpp"
#include <LinkedListNode.hpp>



struct Subscriber : public LinkedListNode<Subscriber>, public PublishInfo {
	// message source, set by subscribe() method
	// todo: type not necessary?
	MessageInfo source;

	// message convert options, depend on endpoint and message types
	ConvertOptions convertOptions;
};
using SubscriberList = LinkedListNode<Subscriber>;
