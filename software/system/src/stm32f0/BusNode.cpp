#include "../BusNode.hpp"
#include "Loop.hpp"
#include "defs.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


// debug
/*
#define initSignal() configureOutput(3)
#define setSignal(value) setOutput(3, value)
#define toggleSignal() toggleOutput(3)
*/
#define initSignal()
#define setSignal(value)
#define toggleSignal()

/*
	Dependencies:
	
	Config:
	
	Resources:
*/
namespace BusNode {

Waitlist<ReceiveParameters> receiveWaitlist;
Waitlist<SendParameters> sendWaitlist;

// event loop handler chain
Loop::Handler nextHandler;
void handle() {

	// call next handler in chain
	BusNode::nextHandler();
}

void init() {
	// check if already initialized
	if (BusNode::nextHandler != nullptr)
		return;
	
	// add to event loop handler chain
	BusNode::nextHandler = Loop::addHandler(handle);
}

Awaitable<ReceiveParameters> receive(int &length, uint8_t *data) {
	assert(BusMaster::nextHandler != nullptr);
	return {BusNode::receiveWaitlist, &length, data};
}

Awaitable<SendParameters> send(int length, uint8_t const *data) {
	assert(BusMaster::nextHandler != nullptr);
	return {BusNode::sendWaitlist, length, data};
}

} // namespace BusNode
