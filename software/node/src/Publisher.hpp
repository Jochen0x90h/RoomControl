#pragma once

#include "Message.hpp"
#include <Coroutine.hpp>
#include <LinkedListNode.hpp>


struct Publisher : public LinkedListNode<Publisher> {
	// endpoint or topic index, set by interface or broker
	uint16_t index;

	// indicates if the data has changed and needs to be published
	bool dirty = false;

	// type of message to be published
	MessageType messageType;

	// message (length is defined by messageType)
	void const *message;
	
	// event to notify the interface that the dirty flag was set (set and owned by the interface or broker)
	Event *event = nullptr;
	
	void publish() {
		this->dirty = true;
		if (this->event != nullptr)
			this->event->set();
	}
};
using PublisherList = LinkedListNode<Publisher>;
