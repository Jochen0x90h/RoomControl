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
		void *config;

		// write data
		int writeCount;
		void const *writeData;

		// read data
		int readCount;
		void *readData;
	};


	virtual ~SpiMaster();

	/**
	 * Transfer data to/from SPI device. Zero length transfers are not supported, i.e. writeCount or readCount must be
	 * greater than zero.
	 * @param writeCount number of bytes to write
	 * @param writeData data to write (driver may require that the data is located in RAM)
	 * @param readCount number of bytes to read
	 * @param readData data to read
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] virtual Awaitable<Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) = 0;

	/**
	 * Write to an SPI device, convenience method.
	 * @param count number of bytes to write
	 * @param data data to write (driver may require that the data is located in RAM)
	 * @return use co_await on return value to await completion
	 */
	[[nodiscard]] inline Awaitable<Parameters> write(int count, void const *data) {
		return transfer(count, data, 0, nullptr);
	}

	/**
	 * Transfer data to/from SPI device and block until finished. Zero length transfers are not supported, i.e.
	 * writeCount or readCount must be greater than zero.
	 * @param writeCount number of bytes to write
	 * @param writeData data to write (driver may require that the data is located in RAM)
	 * @param readCount number of bytes to read
	 * @param readData data to read
	 */
	virtual void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) = 0;
};
