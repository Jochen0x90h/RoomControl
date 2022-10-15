#pragma once

#include <cstdint>
#include <Coroutine.hpp>


namespace Spi {

// Internal helper: Stores the parameters in the awaitable during co_await
struct Parameters {
	// write data
	int writeLength; // commandLength for write-only transfers
	void const *writeData;
	
	// read data
	int readLength; // dataLength for write-only transfers
	void *readData;
};

/**
 * Initialize SPI
 */
#ifdef PLATFORM_POSIX
void init(char const *fileName = "fram.bin");
#else
void init();
#endif

/**
 * Transfer data to/from SPI device
 * @param index index of spi context (number of channels defined by SPI_CONTEXTS in boardConfig.hpp)
 * @param writeCount number of 8/16/32 bit values to write
 * @param writeData array of 8/16/32 bit values to write (ram-only dependent on driver)
 * @param readCount number of 8/16/32 bit values to read
 * @param readData array of 8/16/32 bit values to read (ram-only dependent on driver)
 * @return use co_await on return value to await completion
 */
[[nodiscard]] Awaitable<Parameters> transfer(int index, int writeCount, void const *writeData, int readCount, void *readData);

/**
 * Write a command to an SPI device, e.g. a display, indicating a command using a separate data/command line if supported
 * @param index index of spi context
 * @param writeCount length of data to write
 * @param writeData data to write
 * @return use co_await on return value to await completion
 */
[[nodiscard]] inline Awaitable<Parameters> writeCommand(int index, int writeCount, void const *writeData) {
	return transfer(index, writeCount | 0x80000000, writeData, 0, nullptr);
}

} // namespace Spi
