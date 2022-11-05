#pragma once

#include "../BusMaster.hpp"
#include "Loop.hpp"
#include "../posix/File.hpp"
#include <bus.hpp>
#include <chrono>


class BusMasterImpl : public BusMaster, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	BusMasterImpl();

	Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) override;
	Awaitable<SendParameters> send(int length, uint8_t const *data) override;

	void handle(Gui &gui) override;


	struct Endpoint {
		Array<bus::PlugType const> plugs;
	};

	// persistent state of device
	struct PersistentState {
		int address;
		AesKey aesKey;
		uint32_t securityCounter;
		int index;
	};

	struct Device {
		// device id
		uint32_t id;

		// two endpoint lists to test reconfiguration of device
		Array<Endpoint const> endpoints[2];

		// offset in file and persistent state that is stored in file
		int offset;
		PersistentState persistentState;

		// index to set when commissioned
		uint8_t nextIndex;

		// attribute reading
		bool readAttribute;
		uint8_t endpointIndex;
		bus::Attribute attribute;

		// state
		int states[16];
	};

protected:
	void setHeader(bus::MessageWriter &w, Device &device);
	void sendToMaster(bus::MessageWriter &w, Device &device);

	File file;

	std::chrono::steady_clock::time_point time;

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};
