#pragma once

#include "../BusNode.hpp"
#include "Loop.hpp"
#include <bus.hpp>


class BusNodeEmu : public BusNode, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	BusNodeEmu();

	[[nodiscard]] Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) override;
	[[nodiscard]] Awaitable<SendParameters> send(int length, uint8_t const *data) override;

	void handle(Gui &gui) override;

protected:
	void sendToNode(bus::MessageWriter &w);

	uint32_t deviceId;
	int plugCount;
	bus::PlugType plugs[32];
	int states[32];
	uint32_t securityCounter = 0;

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};
