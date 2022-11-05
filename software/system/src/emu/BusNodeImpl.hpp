#pragma once

#include "../BusNode.hpp"
#include "Loop.hpp"
#include "../posix/File.hpp"
#include <bus.hpp>


class BusNodeImpl : public BusNode, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	BusNodeImpl();

	[[nodiscard]] Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) override;
	[[nodiscard]] Awaitable<SendParameters> send(int length, uint8_t const *data) override;

	void handle(Gui &gui) override;

protected:
	void sendToNode(bus::MessageWriter &w);

	File file;

	struct Device {
		uint32_t deviceId;
		int plugCount;
		bus::PlugType plugs[32];
		uint32_t securityCounter = 0;
	};
	Device device;
	int states[32];

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};
