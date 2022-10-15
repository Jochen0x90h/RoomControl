#pragma once

#include <cstdint>
#include <Coroutine.hpp>


/**
 * Interface to an SPI device
 */
class SpiMaster {
public:

	// Internal helper: Stores the parameters in the awaitable during co_await
	struct Parameters {
		// pointer to configuration
		void const *config;

		// write data
		int writeCount;
		void const *writeData;

		// read data
		int readCount;
		void *readData;
	};


	virtual ~SpiMaster();


	/**
	 * Transfer data to/from SPI device
	 * @param writeCount number of 8/16/32 bit values to write
	 * @param writeData array of 8/16/32 bit values to write (ram-only dependent on driver)
	 * @param readCount number of 8/16/32 bit values to read
	 * @param readData array of 8/16/32 bit values to read (ram-only dependent on driver)
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] virtual Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) = 0;

	/**
	 * Write a command to an SPI device, e.g. a display, indicating a command using a separate data/command line if supported
	 * @param index index of spi context
	 * @param writeCount length of data to write
	 * @param writeData data to write
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] inline Awaitable<Parameters> writeCommand(int count, void const *command) {
		return transfer(count | 0x80000000, command, 0, nullptr);
	}

	[[nodiscard]] inline Awaitable<Parameters> writeData(int count, void const *data) {
		return transfer(count, data, 0, nullptr);
	}

	virtual void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) = 0;
};
