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
int endpointCount;
bus::EndpointType endpoints[32];
int states[32];
uint32_t securityCounter = 0;


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
		auto a1 = r.arbiter();
		if (a1 == 0) {
			// node has sent an enumerate message

			// get encoded device id
			uint32_t deviceId = r.id();
			BusNode::deviceId = deviceId;

			// check message integrity code (mic)
			r.setMessageFromEnd(micLength);
			Nonce nonce(deviceId, 0);
			if (!r.decrypt(micLength, nonce, bus::defaultAesKey))
				return true;

			// get endpoints
			BusNode::endpointCount = r.getRemaining();
			array::copy(BusNode::endpointCount, BusNode::endpoints, r.current);


			// reply with commission message
			bus::MessageWriter w(data);
			w.setHeader();

			w.u8(0);
			w.u8(0);

			// device id
			w.u32L(deviceId);

			// address that gets assigned to the device
			w.u8(5);

			// key
			w.data(bus::defaultKey);

			// only add message integrity code (mic) using default key, message stays unencrypted
			w.setMessage();
			//Nonce nonce(deviceId, 0);
			w.encrypt(micLength, nonce, bus::defaultAesKey);

			int writeLength = w.getLength();

			BusNode::receiveWaitlist.resumeFirst([data, writeLength](ReceiveParameters &p) {
				int length = min(writeLength, *p.receiveLength);
				array::copy(length, p.receiveData, data);
				*p.receiveLength = length;
				return true;
			});
		} else {
			// node has sent a data message
			int address = a1 - 1 + (r.arbiter() << 3);
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

			// handle endpoint/state
			uint8_t endpointIndex = r.u8() + 1;
			auto endpointType = BusNode::endpoints[endpointIndex];
			uint8_t state = r.u8();
			switch (endpointType) {
				case bus::EndpointType::OFF_ON_IN:
				case bus::EndpointType::OFF_ON_TOGGLE_IN:
					if (state <= 1)
						BusNode::states[endpointIndex] = state;
					else
						BusNode::states[endpointIndex] ^= 1;
					break;
				case bus::EndpointType::UP_DOWN_IN:
					BusNode::states[endpointIndex] = state;
					break;
			}

			// loopback
			bus::MessageWriter w(data);

			// set start of header
			w.setHeader();

			// encoded address
			w.arbiter((address & 7) + 1);
			w.arbiter(address >> 3);

			// security counter
			w.u32L(BusNode::securityCounter);

			// set start of message
			w.setMessage();

			// endpoint index
			w.u8(endpointIndex);

			// message data
			w.u8(BusNode::states[endpointIndex]);

			// encrypt
			Nonce nonce2(address, BusNode::securityCounter);
			w.encrypt(micLength, nonce2, bus::defaultAesKey);

			// increment security counter
			++BusNode::securityCounter;

			int writeLength = w.getLength();

			BusNode::receiveWaitlist.resumeFirst([data, writeLength](ReceiveParameters &p) {
				int length = min(writeLength, *p.receiveLength);
				array::copy(length, p.receiveData, data);
				*p.receiveLength = length;
				return true;
			});
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
