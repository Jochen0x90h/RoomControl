#include "BusNodeImpl.hpp"
#include <bus.hpp>
#include <util.hpp>


constexpr int micLength = 4;

BusNodeImpl::BusNodeImpl() : file("busNode.bin", File::Mode::READ_WRITE) {
	// load device data
	int size = this->file.read(0, sizeof(Device), &this->device);
	if (size != sizeof(Device)) {
		this->device.deviceId = 0;
		this->device.plugCount = 0;
		this->device.securityCounter = 0;
	}

	// add to list of handlers
	Loop::handlers.add(*this);
}

Awaitable<BusNode::ReceiveParameters> BusNodeImpl::receive(int &length, uint8_t *data) {
	return {this->receiveWaitlist, &length, data};
}

Awaitable<BusNode::SendParameters> BusNodeImpl::send(int length, uint8_t const *data) {
	return {this->sendWaitlist, length, data};
}

void BusNodeImpl::handle(Gui &gui) {
	// handle pending send operations
	this->sendWaitlist.resumeFirst([this](SendParameters &p) {
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
			this->device.deviceId = deviceId;

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

				// only add message integrity code (mic) using default key, message itself stays unencrypted
				w.setMessage();
				//Nonce nonce(deviceId, 0);
				w.encrypt(micLength, nonce, bus::defaultAesKey);

				// clear security counter
				this->device.securityCounter = 1;

				sendToNode(w);
			}

			// get list of plugs: request PLUG_LIST attribute
			{
				bus::MessageWriter w(data);
				w.setHeader();

				// encoded address
				w.address(5);

				// security counter
				w.u32L(this->device.securityCounter);

				// set start of message
				w.setMessage();

				// "escaped" endpoint index
				w.u8(255);
				w.u8(0);

				// attribute
				w.e8(bus::Attribute::PLUG_LIST);

				// no message data to indicate that we want to read the attribute

				// encrypt
				Nonce nonce(5, this->device.securityCounter);
				w.encrypt(micLength, nonce, bus::defaultAesKey);

				// increment security counter
				++this->device.securityCounter;

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
			if (endpointIndex == 255) {
				// attribute
				endpointIndex = r.u8();
				auto attribute = r.e8<bus::Attribute>();
				if (attribute == bus::Attribute::PLUG_LIST) {
					this->device.plugCount = r.getRemaining() / 2;
					r.data16L(this->device.plugCount, this->device.plugs);

					// write device data
					this->file.write(0, sizeof(Device), &this->device);
				}
			} else {
				// plug
				uint8_t plugIndex = r.u8();

				// handle pug/state
				uint8_t nextPlugIndex = plugIndex + 1;
				auto plugType = this->device.plugs[nextPlugIndex];
				uint8_t state = r.u8();
				switch (plugType) {
				case bus::PlugType::BINARY_POWER_LIGHT_IN:
					if (state <= 1)
						this->states[nextPlugIndex] = state;
					else
						this->states[nextPlugIndex] ^= 1;
					break;
				case bus::PlugType::TERNARY_OPENING_BLIND_IN:
					this->states[nextPlugIndex] = state;
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
				w.u32L(this->device.securityCounter);

				// set start of message
				w.setMessage();

				// endpoint index
				w.u8(endpointIndex);

				// plug index
				w.u8(nextPlugIndex);

				// message data
				w.u8(this->states[nextPlugIndex]);

				// encrypt
				Nonce nonce2(address, this->device.securityCounter);
				w.encrypt(micLength, nonce2, bus::defaultAesKey);

				// increment security counter
				++this->device.securityCounter;

				// write security counter
				this->file.write(offsetof(Device, securityCounter), 4, &this->device.securityCounter);

				sendToNode(w);
			}
		}
		return true;
	});
}

void BusNodeImpl::sendToNode(bus::MessageWriter &w) {
	int length = w.getLength();
	auto data = w.begin;
	this->receiveWaitlist.resumeFirst([length, data](ReceiveParameters &p) {
		int len = min(length, *p.receiveLength);
		array::copy(len, p.receiveData, data);
		*p.receiveLength = len;
		return true;
	});
}
