#include "BusNodeImpl.hpp"


BusNodeImpl::BusNodeImpl() {
	// add to list of handlers
	Loop::handlers.add(*this);
}

Awaitable<BusNode::ReceiveParameters> BusNodeImpl::receive(int &length, uint8_t *data) {
	return {this->receiveWaitlist, &length, data};
}

Awaitable<BusNode::SendParameters> BusNodeImpl::send(int length, uint8_t const *data) {
	return {this->sendWaitlist, length, data};
}

void BusNodeImpl::handle() {

}
