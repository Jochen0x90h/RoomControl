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
#ifdef EMU
void init(char const *fileName = "fram.bin");
#else
void init();
#endif

/**
 * Transfer data to/from SPI device
 * @param index index of spi context (number of channels defined by SPI_CONTEXTS in boardConfig.hpp)
 * @param writeLength length of data to write
 * @param writeData data to write (8/16/32 bit and ram-only dependent on driver)
 * @param readLength length of data to read
 * @param readData data to read (8/16/32 bit and ram-only dependent on driver)
 * @param command true if this is a command which gets indicated on a separate pin
 * @return use co_await on return value to await completion
 */
[[nodiscard]] Awaitable<Parameters> transfer(int index, int writeLength, void const *writeData, int readLength, void *readData);

/**
 * Write a command to an SPI device, e.g. a display, indicating a command using a separate data/command line if supported
 * @param index index of spi context
 * @param writeLength length of data to write
 * @param writeData data to write
 * @return use co_await on return value to await completion
 */
[[nodiscard]] inline Awaitable<Parameters> writeCommand(int index, int writeLength, void const *writeData) {
	return transfer(index, writeLength | 0x80000000, writeData, 0, nullptr);
}

} // namespace Spi
