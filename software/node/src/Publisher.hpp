#pragma once

#include "Message.hpp"
#include <Coroutine.hpp>


struct PublishInfo {
	// message destination
	MessageInfo destination;

	struct Parameters {
		// info about the message source for the subscriber to identify the message
		MessageInfo &info;

		// message (length is defined by Subscriber::messageType)
		void *message;
	};

	// barrier where the subscriber waits for new messages (owned by subscriber)
	using Barrier = ::Barrier<Parameters>;
	Barrier *barrier = nullptr;
};


struct Publisher : public PublishInfo {
	uint8_t id;
	MessageType srcType;

	ConvertOptions convertOptions;

	void setInfo(PublishInfo const &publishInfo) {
		*static_cast<PublishInfo *>(this) = publishInfo;
	}

	void publishSwitchCommand(uint8_t src) {
		if (this->barrier == nullptr)
			return;
		this->barrier->resumeFirst([this, src] (PublishInfo::Parameters &p) {
			p.info = this->destination;

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			return convertCommand(this->destination.type, dst, src, this->convertOptions);
		});
	}
	void publishFloatValue(float src) {
		if (this->barrier == nullptr)
			return;
		this->barrier->resumeFirst([this, src] (PublishInfo::Parameters &p) {
			p.info = this->destination;

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message *>(p.message);
			return convertFloatValue(this->destination.type, dst, this->srcType, src, this->convertOptions);
		});
	}
};
