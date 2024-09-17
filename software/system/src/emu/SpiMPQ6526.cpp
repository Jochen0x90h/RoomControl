#include "SpiMPQ6526.hpp"
#include <Terminal.hpp>
#include <StringOperators.hpp>


SpiMPQ6526::SpiMPQ6526()
	: relayStates{Gui::LightState::DISABLED, Gui::LightState::DISABLED, Gui::LightState::DISABLED, Gui::LightState::DISABLED}
{
	// add to list of handlers
	loop::handlers.add(*this);
}

Awaitable <SpiMaster::Parameters> SpiMPQ6526::transfer(int writeCount, void const *writeData, int readCount, void *readData) {
	return {this->waitlist, nullptr, writeCount, writeData, readCount, readData};
}

void SpiMPQ6526::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	writeCount &= 0x7fffffff;
	auto w = reinterpret_cast<uint16_t const *>(writeData);
	auto r = reinterpret_cast<uint16_t *>(readData);

	bool driverEnable = (w[0] & 0x8000) != 0;
	if (driverEnable) {
		Terminal::out << "relays ";
		updateState(0, w[0], 1, 3);
		updateState(1, w[0], 5, 3);
		updateState(2, w[0], 7, 9);
		updateState(3, w[0], 11, 9);
		Terminal::out << '\n';
	}

	// todo: read data
}

void SpiMPQ6526::handle(Gui &gui) {
	this->waitlist.resumeFirst([this](Parameters &p) {
		transferBlocking(p.writeCount, p.writeData, p.readCount, p.readData);
		return true;
	});

	// draw relay driver on gui
	gui.light(this->relayStates[0]);
	gui.light(this->relayStates[1]);
	gui.light(this->relayStates[2]);
	gui.light(this->relayStates[3]);
}


void SpiMPQ6526::updateState(int index, uint16_t data, int i1, int i2) {
	int x = (data >> i1) & 3;
	int y = (data >> i2) & 3;

	// high and low side of a half-bridge should never be on at the same time
	assert(x != 3 && y != 3);

	if ((x == 1 && y == 2) || (x == 2 && y == 1)) {
		auto state = x == 2 ? Gui::LightState::ON : Gui::LightState::OFF;
		if (state != this->relayStates[index])
			Terminal::out << dec(index) << (state == Gui::LightState::ON ? ":on " : ":off ");
		this->relayStates[index] = state;
	}
}
