#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"


class SpiBME680 : public SpiMaster, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	SpiBME680();

	Awaitable <Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
	void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	void handle(Gui &gui) override;

	void setTemperature(float celsius) {
		int value = int(celsius * 5120.0f);
		this->airSensorRegisters[0x1D] = 1 << 7; // flag indicating new data
		this->airSensorRegisters[0x24] = value << 4;
		this->airSensorRegisters[0x23] = value >> 4;
		this->airSensorRegisters[0x22] = value >> 12;
	}


	uint8_t airSensorRegisters[256];

	// pointer to lower or upper page in the air sensor registers
	uint8_t *page;

	Waitlist<SpiMaster::Parameters> waitlist;
};
