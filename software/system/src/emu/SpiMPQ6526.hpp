#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"


/**
 * Emulates a MPQ6526 motor/relay driver
 */
class SpiMPQ6526 : public SpiMaster, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	SpiMPQ6526();

	Awaitable <Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
	void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	void handle(Gui &gui) override;

	void updateState(int index, uint16_t data, int i1, int i2);


	Gui::LightState relayStates[4];
	Waitlist<SpiMaster::Parameters> waitlist;
};