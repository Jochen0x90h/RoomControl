#include <bus.hpp>
#include <BusNode.hpp>
#include <crypt.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <util.hpp>
#include <emu/Gui.hpp>
#include <emu/Loop.hpp>
#include <iostream>
#include <fstream>


// emulator implementation of bus uses virtual devices on user interface
namespace BusNode {


// event loop handler chain
Loop::Handler nextHandler;
void handle(Gui &gui) {

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
	return {};
}

Awaitable<SendParameters> send(int length, uint8_t const *data) {
	return {};
}

} // namespace BusNode
