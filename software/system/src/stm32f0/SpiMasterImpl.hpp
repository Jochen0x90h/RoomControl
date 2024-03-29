#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include "gpio.hpp"


/**
 * Implementation of SPI hardware interface for nrf52 platform with multiple virtual channels
 */
class SpiMasterImpl : public Loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param index index of device, 1-2 for SPI1 or SPI2
	 * @param sckPin clock pin (SPI1: PA5 or PB3)
	 * @param mosiPin master out slave in pin (SPI1: PA7 or PB5)
	 * @param misoPin master in slave out pin (SPI1: PA6 or PB4)
	 */
	SpiMasterImpl(int index, int sckPin, int mosiPin, int misoPin);

	void handle() override;

	/**
	 * Virtual channel to a slave device using a dedicated CS pin
	 */
	class Channel : public SpiMaster {
		friend class SpiMasterImpl;
	public:
		/**
		 * Constructor
		 * @param device the SPI device to operate on
		 * @param csPin chip select pin of the slave
		 */
		Channel(SpiMasterImpl &master, int csPin);

		Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
		void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	protected:
		SpiMasterImpl &master;
		int csPin;
	};

protected:
	void startTransfer(SpiMaster::Parameters const &p);


	int csPin;
	Waitlist<SpiMaster::Parameters> waitlist;
};
