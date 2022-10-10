#include "Subscriber.hpp"

static void setInfo(SubscriberParameters &p, Subscriber const &subscriber) {
	p.info.connectionIndex = subscriber.connectionIndex;
	p.info.deviceId = subscriber.deviceId;
	p.info.plugIndex = subscriber.data->destination.plugIndex;
	//p.info.type = subscriber.info.type;
}

void SubscriberList::publishSwitch(uint8_t plugIndex, uint8_t value) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.target.barrier->resumeFirst([&subscriber, value](SubscriberParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertSwitch(subscriber.target.type, dst, value, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishInt8(uint8_t plugIndex, int8_t value) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.target.barrier->resumeFirst([&subscriber, value](SubscriberParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertInt8(subscriber.target.type, dst, value, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloat(uint8_t plugIndex, float value) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.target.barrier->resumeFirst([&subscriber, value](SubscriberParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloat(subscriber.target.type, dst, value, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatCommand(uint8_t plugIndex, float value, uint8_t command) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.target.barrier->resumeFirst([&subscriber, value, command](SubscriberParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatCommand(subscriber.target.type, dst, value, command, subscriber.data->convertOptions);
			});
		}
	}
}

void SubscriberList::publishFloatTransition(uint8_t plugIndex, float value, uint8_t command, uint16_t transition) {
	for (auto &subscriber: *this) {
		// check if this is the right plug
		if (subscriber.data->source.plugIndex == plugIndex) {
			// resume subscriber
			subscriber.target.barrier->resumeFirst([&subscriber, value, command, transition](SubscriberParameters &p) {
				setInfo(p, subscriber);

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				return convertFloatTransition(subscriber.target.type, dst,
					value, command, transition, subscriber.data->convertOptions);
			});
		}
	}
}


void ListenerList::publishSwitch(uint8_t deviceId, uint8_t plugIndex, uint8_t value) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([this, deviceId, plugIndex, value](ListenerParameters &p) {
			p.info = {this->interfaceId, deviceId, plugIndex};

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.u8 = value;
			return true;
		});
	}
}

void ListenerList::publishInt8(uint8_t deviceId, uint8_t plugIndex, int8_t value) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([this, deviceId, plugIndex, value](ListenerParameters &p) {
			p.info = {this->interfaceId, deviceId, plugIndex};

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.i8 = value;
			return true;
		});
	}
}

void ListenerList::publishFloat(uint8_t deviceId, uint8_t plugIndex, float value) {
	for (auto &listener: *this) {
		// resume subscriber
		listener.barrier->resumeFirst([this, deviceId, plugIndex, value](ListenerParameters &p) {
			p.info = {this->interfaceId, deviceId, plugIndex};

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
		listener.barrier->resumeFirst([this, deviceId, plugIndex, value, command](ListenerParameters &p) {
			p.info = {this->interfaceId, deviceId, plugIndex};

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
		listener.barrier->resumeFirst([this, deviceId, plugIndex, value, command, transition](ListenerParameters &p) {
			p.info = {this->interfaceId, deviceId, plugIndex};

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			dst.value.f32 = value;
			dst.command = command;
			dst.transition = transition;
			return true;
		});
	}
}
