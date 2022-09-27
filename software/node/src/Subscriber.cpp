#include "Subscriber.hpp"

static void setInfo(MessageParameters &p, Subscriber const &subscriber) {
	p.info.sourceIndex = subscriber.connectionIndex;
	p.info.deviceId = subscriber.deviceId;
	p.info.plugIndex = subscriber.data->destination.plugIndex;
	//p.info.type = subscriber.info.type;
}

void SubscriberList::publishSwitch(uint8_t plugIndex, uint8_t value) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, value](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertSwitch(subscriber.info.type, dst, value, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloat(uint8_t plugIndex, float value) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, value](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloat(subscriber.info.type, dst, value, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatCommand(uint8_t plugIndex, float value, uint8_t command) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, value, command](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatCommand(subscriber.info.type, dst, value, command, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatTransition(uint8_t plugIndex, float value, uint8_t command, uint16_t transition) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.info.barrier->resumeFirst([&subscriber, value, command, transition](MessageParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatTransition(subscriber.info.type, dst,
					value, command, transition, subscriber.data->convertOptions);
			});
		}
	}
}


static void setInfo(MessageParameters &p, Listener const &listener, uint8_t deviceId, uint8_t plugIndex) {
	p.info.sourceIndex = listener.sourceIndex;
	p.info.deviceId = deviceId;
	p.info.plugIndex = plugIndex;
}

void ListenerList::publishSwitch(uint8_t deviceId, uint8_t plugIndex, uint8_t value) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([&listener, deviceId, plugIndex, value](MessageParameters &p) {
			setInfo(p, listener, deviceId, plugIndex);

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.u8 = value;
			return true;
		});
	}
}

void ListenerList::publishFloat(uint8_t deviceId, uint8_t plugIndex, float value) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([&listener, deviceId, plugIndex, value](MessageParameters &p) {
			setInfo(p, listener, deviceId, plugIndex);

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.f32 = value;
			return true;
		});
	}
}

void ListenerList::publishFloatCommand(uint8_t deviceId, uint8_t plugIndex, float value, uint8_t command) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([&listener, deviceId, plugIndex, value, command](MessageParameters &p) {
			setInfo(p, listener, deviceId, plugIndex);

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.f32 = value;
			dst.command = command;
			return true;
		});
	}
}

void ListenerList::publishFloatTransition(uint8_t deviceId, uint8_t plugIndex, float value, uint8_t command, uint16_t transition) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([&listener, deviceId, plugIndex, value, command, transition](MessageParameters &p) {
			setInfo(p, listener, deviceId, plugIndex);

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.f32 = value;
			dst.command = command;
			dst.transition = transition;
			return true;
		});
	}
}
