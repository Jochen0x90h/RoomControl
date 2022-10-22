#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include "gpio.hpp"


/**
 * Implementation of SPI hardware interface for nrf52 platform with multiple virtual channels
 */
class SpiMasterDevice : public Loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param index index of device, 1-2 for SPI1 or SPI2
	 * @param sckPin clock pin (SPI1: PA5 or PB3)
	 * @param mosiPin master out slave in pin (SPI1: PA7 or PB5)
	 * @param misoPin master in slave out pin (SPI1: PA6 or PB4)
	 */
	SpiMasterDevice(int index, int sckPin, int mosiPin, int misoPin);

	void handle() override;

	/**
	 * Virtual channel to a slave device using a dedicated CS pin
	 */
	class Channel : public SpiMaster {
		friend class SpiMasterDevice;
	public:
		/**
		 * Constructor
		 * @param device the SPI device to operate on
		 * @param csPin chip select pin of the slave
		 */
		Channel(SpiMasterDevice &device, int csPin);

		Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
		void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	protected:
		SpiMasterDevice &device;
		int csPin;
	};

protected:
	void startTransfer(SpiMaster::Parameters const &p);


	int csPin;
	Waitlist<SpiMaster::Parameters> waitlist;
};
