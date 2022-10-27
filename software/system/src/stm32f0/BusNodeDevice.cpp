#include "BusNodeDevice.hpp"


BusNodeDevice::BusNodeDevice() {
	// add to list of handlers
	Loop::handlers.add(*this);
}

Awaitable<BusNode::ReceiveParameters> BusNodeDevice::receive(int &length, uint8_t *data) {
	return {this->receiveWaitlist, &length, data};
}

Awaitable<BusNode::SendParameters> BusNodeDevice::send(int length, uint8_t const *data) {
	return {this->sendWaitlist, length, data};
}

void BusNodeDevice::handle() {

}
