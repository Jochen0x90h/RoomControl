#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include "gpio.hpp"


/**
 * Implementation of SPI hardware interface for stm32f0 that transfers a single 16 bit word
 */
class SpiMasterSingle16 : public SpiMaster, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param index index of device, 1-2 for SPI1 or SPI2
	 * @param sckPin clock pin (SPI1: PA5 or PB3)
	 * @param mosiPin master out slave in pin (SPI1: PA7 or PB5)
	 * @param misoPin master in slave out pin (SPI1: PA6 or PB4)
	 */
	SpiMasterSingle16(int sckPin, int mosiPin, int misoPin, int csPin);

	void handle() override;

	// assumes writeCount and readCount to be 2
	Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;

	// not implemented
	void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

protected:
	void startTransfer(SpiMaster::Parameters const &p);


	int csPin;

	Waitlist<SpiMaster::Parameters> waitlist;
};
