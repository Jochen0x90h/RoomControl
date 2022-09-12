#include "Subscriber.hpp"

static void setInfo(SubscriberInfo::Parameters &p, Subscriber const &subscriber) {
	p.info.deviceId = subscriber.data->destination.deviceId;
	p.info.plugIndex = subscriber.data->destination.plugIndex;
	p.info.connectionIndex = subscriber.connectionIndex;
}

void SubscriberList::publishSwitch(int plugIndex, uint8_t message) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message](SubscriberInfo::Parameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertSwitch(subscriber.info.type, dst, message, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloat(int plugIndex, float message) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message](SubscriberInfo::Parameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloat(subscriber.info.type, dst, message, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatCommand(int plugIndex, float message, uint8_t command) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message, command](SubscriberInfo::Parameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatCommand(subscriber.info.type, dst, message, command, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatTransition(int plugIndex, float message, uint8_t command, uint16_t transition) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message, command, transition](SubscriberInfo::Parameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatTransition(subscriber.info.type, dst,
					message, command, transition, subscriber.data->convertOptions);
			});
		}
	}
}
