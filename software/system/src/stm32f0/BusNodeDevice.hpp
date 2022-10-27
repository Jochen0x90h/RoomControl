#pragma once

#include "../BusNode.hpp"
#include "Loop.hpp"


class BusNodeDevice : public BusNode, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	BusNodeDevice();

	[[nodiscard]] Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) override;
	[[nodiscard]] Awaitable<SendParameters> send(int length, uint8_t const *data) override;

	void handle() override;

protected:
	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};
