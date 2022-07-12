#include "../BusNode.hpp"
#include <bus.hpp>
#include <util.hpp>
#include <emu/Gui.hpp>
#include <emu/Loop.hpp>
#include <iostream>
#include <fstream>


// emulator implementation of bus uses virtual devices on user interface
namespace BusNode {

Waitlist<ReceiveParameters> receiveWaitlist;
Waitlist<SendParameters> sendWaitlist;


constexpr int micLength = 4;
uint32_t deviceId;
int plugCount;
bus::PlugType plugs[32];
int states[32];
uint32_t securityCounter = 0;


void sendToNode(bus::MessageWriter &w) {
	int length = w.getLength();
	auto data = w.begin;
	BusNode::receiveWaitlist.resumeFirst([length, data](ReceiveParameters &p) {
		int len = min(length, *p.receiveLength);
		array::copy(len, p.receiveData, data);
		*p.receiveLength = len;
		return true;
	});
}


// event loop handler chain
Loop::Handler nextHandler;
void handle(Gui &gui) {
	// handle pending send operations
	BusNode::sendWaitlist.resumeFirst([](SendParameters &p) {
		if (p.sendLength == 0)
			return true;

		uint8_t data[64];
		array::copy(p.sendLength, data, p.sendData);

		// read message
		bus::MessageReader r(p.sendLength, data);
		r.setHeader();

		// get address
		if (r.peekU8() == 0) {
			// node has sent an enumerate message
			r.u8();

			// get encoded device id
			uint32_t deviceId = r.id();
			BusNode::deviceId = deviceId;

			// check message integrity code (mic)
			r.setMessageFromEnd(micLength);
			Nonce nonce(deviceId, 0);
			if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
				return true;

			// protocol version
			r.u8();

			// endpoint count
			r.u8();

			// reply with commission message
			{
				bus::MessageWriter w(data);
				w.setHeader();

				// prefix
				w.u8(0);
				w.u8(0);

				// device id
				w.u32L(deviceId);

				// address that gets assigned to the device
				w.u8(5);

				// key
				w.data8(bus::defaultKey);

				// only add message integrity code (mic) using default key, message stays unencrypted
				w.setMessage();
				//Nonce nonce(deviceId, 0);
				w.encrypt(micLength, nonce, bus::defaultAesKey);

				sendToNode(w);
			}

			// get list of plugs
			{
				bus::MessageWriter w(data);
				w.setHeader();

				// encoded address
				w.address(5);

				// security counter
				w.u32L(securityCounter);

				// set start of message
				w.setMessage();

				// endpoint index
				w.u8(0);

				// attribute
				w.e8(bus::Attribute::PLUG_LIST);

				// no message data to indicate that we want to read the attribute

				// encrypt
				Nonce nonce(5, securityCounter);
				w.encrypt(micLength, nonce, bus::defaultAesKey);

				// increment security counter
				++securityCounter;

				sendToNode(w);
			}
		} else {
			// node has sent a data message
			int address = r.address();
			if (address != 5)
				return true;

			// get security counter
			uint32_t securityCounter = r.u32L();

			// set start of encrypted message
			r.setMessage();

			// decrypt
			Nonce nonce(address, securityCounter);
			if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
				return true;

			uint8_t endpointIndex = r.u8();
			uint8_t attributeOrPlugIndex = r.u8();

			if (attributeOrPlugIndex >= uint8_t(bus::Attribute::FIRST_ATTRIBUTE)) {
				// attribute
				auto attribute = bus::Attribute(attributeOrPlugIndex);
				if (attribute == bus::Attribute::PLUG_LIST) {
					BusNode::plugCount = r.getRemaining() / 2;
					r.data16L(BusNode::plugCount, BusNode::plugs);
				}
			} else {
				// plug
				uint8_t plugIndex = attributeOrPlugIndex;

				// handle pug/state
				uint8_t nextPlugIndex = plugIndex + 1;
				auto plugType = BusNode::plugs[nextPlugIndex];
				uint8_t state = r.u8();
				switch (plugType) {
				case bus::PlugType::BINARY_POWER_LIGHT_IN:
					if (state <= 1)
						BusNode::states[nextPlugIndex] = state;
					else
						BusNode::states[nextPlugIndex] ^= 1;
					break;
				case bus::PlugType::TERNARY_OPENING_BLIND_IN:
					BusNode::states[nextPlugIndex] = state;
					break;
				default:;
				}

				// loopback
				bus::MessageWriter w(data);

				// set start of header
				w.setHeader();

				// encoded address
				w.address(address);

				// security counter
				w.u32L(BusNode::securityCounter);

				// set start of message
				w.setMessage();

				// endpoint index
				w.u8(endpointIndex);

				// plug index
				w.u8(nextPlugIndex);

				// message data
				w.u8(BusNode::states[nextPlugIndex]);

				// encrypt
				Nonce nonce2(address, BusNode::securityCounter);
				w.encrypt(micLength, nonce2, bus::defaultAesKey);

				// increment security counter
				++BusNode::securityCounter;

				sendToNode(w);
			}
		}
		return true;
	});


	// call next handler in chain
	BusNode::nextHandler(gui);
}

void init() {
	// check if already initialized
	if (BusNode::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	BusNode::nextHandler = Loop::addHandler(handle);
}

Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) {
	assert(BusNode::nextHandler != nullptr);
	return {BusNode::receiveWaitlist, &length, data};
}

Awaitable<SendParameters> send(int length, uint8_t const *data) {
	assert(BusNode::nextHandler != nullptr);
	return {BusNode::sendWaitlist, length, data};
}

} // namespace BusNode
