#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include "gpio.hpp"

/**
 * Implementation of SPI hardware interface for nrf52 platform
 */
class SpiMasterDevice : public Loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param index index of device, 1-2 for SPI1 or SPI2
	 * @param sckPin clock pin (must be PA5 or PB3)
	 * @param mosiPin master out slave in pin (must be PA7)
	 * @param misoPin master in slave out pin (must be PA6)
	 */
	SpiMasterDevice(int index, int sckPin, int mosiPin, int misoPin);

	void handle() override;

	void startTransfer(SpiMaster::Parameters const &p);


	int csPin;
	Waitlist<SpiMaster::Parameters> waitlist;


	/**
	 * Virtual channel to a slave device using a dedicated CS pin
	 */
	class Channel : public SpiMaster {
	public:
		/**
		 * Constructor
		 * @param device the SPI device to operate on
		 * @param csPin chip select pin of the slave
		 */
		Channel(SpiMasterDevice &device, int csPin);

		Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
		void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;


		SpiMasterDevice &device;
		int csPin;
	};
};
