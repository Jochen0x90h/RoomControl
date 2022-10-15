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
	 * @param index index of device, 0-3 for NRF_SPIM0 to NRF_SPIM3. Note that only NRF_SPIM3 supports DC pin
	 * @param sckPin clock pin
	 * @param mosiPin master out slave in pin
	 * @param misoPin master in slave out pin
	 * @param dcPin data/command pin e.g. for displays
	 */
	SpiMasterDevice(int index, int sckPin, int mosiPin, int misoPin, int dcPin = gpio::DISCONNECTED);

	void handle() override;

	void startTransfer(const SpiMaster::Parameters &p);


	int misoPin;
	bool sharedPin;
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
		 * @param writeOnly ture if we only write data and can use MISO as DC (data/command) signal (e.g. for a display)
		 */
		Channel(SpiMasterDevice &device, int csPin, bool writeOnly = false);

		Awaitable<Parameters> transfer(int writeCount, const void *writeData, int readCount, void *readData) override;
		void transferBlocking(int writeCount, const void *writeData, int readCount, void *readData) override;


		SpiMasterDevice &device;
		int csPin;
		bool writeOnly;
	};
};
