#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"
#include "gpio.hpp"

/**
 * Implementation of SPI hardware interface for nrf52 with multiple virtual channels
 */
class SpiMasterImpl : public loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param index index of device, 0-3 for NRF_SPIM0 to NRF_SPIM3. Note that only NRF_SPIM3 supports DC pin
	 * @param sckPin clock pin
	 * @param mosiPin master out slave in pin
	 * @param misoPin master in slave out pin
	 * @param dcPin data/command pin e.g. for displays, can be same as MISO for read-only devices
	 */
	SpiMasterImpl(int sckPin, int mosiPin, int misoPin, int dcPin = gpio::DISCONNECTED);

	void handle() override;

	void startTransfer(const SpiMaster::Parameters &p);


	int misoPin;
	bool sharedPin;

	// list for coroutines waiting for transfer to complete
	Waitlist<SpiMaster::Parameters> waitlist;


	/**
	 * Virtual channel to a slave device using a dedicated CS pin
	 */
	class Channel : public SpiMaster {
	public:
		// mode of data/command signal
		enum class Mode {
			NONE,
			COMMAND, // set DC low
			DATA // set DC high
		};

		/**
		 * Constructor
		 * @param device the SPI device to operate on
		 * @param csPin chip select pin of the slave
		 * @param mode mode of data/command pin
		 */
		Channel(SpiMasterImpl &master, int csPin, Mode mode = Mode::NONE);

		Awaitable<Parameters> transfer(int writeCount, const void *writeData, int readCount, void *readData) override;
		void transferBlocking(int writeCount, const void *writeData, int readCount, void *readData) override;


		SpiMasterImpl &master;
		int csPin;
		Mode mode;
	};
};
