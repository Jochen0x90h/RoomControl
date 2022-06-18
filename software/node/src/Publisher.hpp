#pragma once

#include "Message.hpp"
#include <Coroutine.hpp>


struct PublishInfo {
	// message destination
	MessageInfo2 destination;

	struct Parameters {
		// info about the message source for the subscriber to identify the message
		MessageInfo2 &info;

		// message (length is defined by Subscriber::messageType)
		void *message;
	};

	// barrier where the subscriber waits for new messages (owned by subscriber)
	using Barrier = ::Barrier<Parameters>;
	Barrier *barrier = nullptr;
};


struct Publisher : public PublishInfo {
	uint8_t id;
	MessageType2 srcType;

	ConvertOptions convertOptions;

	void setInfo(PublishInfo const &publishInfo) {
		*static_cast<PublishInfo *>(this) = publishInfo;
	}

	void publishSwitch(uint8_t src) {
		if (this->barrier == nullptr)
			return;
		this->barrier->resumeFirst([this, src] (PublishInfo::Parameters &p) {
			p.info = this->destination;

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message2 *>(p.message);
			return convertSwitch(this->destination.type, dst, src, this->convertOptions);
		});
	}
	void publishFloat(float src) {
		if (this->barrier == nullptr)
			return;
		this->barrier->resumeFirst([this, src] (PublishInfo::Parameters &p) {
			p.info = this->destination;

			// convert to destination message type and resume coroutine if conversion was successful
			auto &dst = *reinterpret_cast<Message2 *>(p.message);
			return convertFloat(this->destination.type, dst, src, this->convertOptions);
		});
	}
};
