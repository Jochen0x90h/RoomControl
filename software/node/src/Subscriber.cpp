#include "Subscriber.hpp"

static void setInfo(MessageParameters &p, Subscriber const &subscriber) {
	p.info.sourceIndex = subscriber.connectionIndex;
	p.info.deviceId = subscriber.deviceId;
	p.info.plugIndex = subscriber.data->destination.plugIndex;
	//p.info.type = subscriber.info.type;
}

void SubscriberList::publishSwitch(uint8_t plugIndex, uint8_t message) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertSwitch(subscriber.info.type, dst, message, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloat(uint8_t plugIndex, float message) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloat(subscriber.info.type, dst, message, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatCommand(uint8_t plugIndex, float message, uint8_t command) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message, command](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatCommand(subscriber.info.type, dst, message, command, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatTransition(uint8_t plugIndex, float message, uint8_t command, uint16_t transition) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, message, command, transition](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatTransition(subscriber.info.type, dst,
					message, command, transition, subscriber.data->convertOptions);
			});
		}
	}
}

static void setInfo(MessageParameters &p, Listener const &listener, uint8_t deviceId, uint8_t plugIndex) {
	p.info.sourceIndex = listener.sourceIndex;
	p.info.deviceId = deviceId;
	p.info.plugIndex = plugIndex;
}

void ListenerList::publishSwitch(uint8_t deviceId, uint8_t plugIndex, uint8_t message) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([&listener, deviceId, plugIndex, message](MessageParameters &p) {
			setInfo(p, listener, deviceId, plugIndex);

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.u8 = message;
			return true;
		});
	}
}
