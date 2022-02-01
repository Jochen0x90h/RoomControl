#pragma once

#include <cstdint>
#include <Coroutine.hpp>


namespace spi {

// Internal helper: Stores the parameters in the awaitable during co_await
struct Parameters {
	// write data
	int writeLength;
	uint8_t const *writeData;
	
	// read data
	int readLength;
	uint8_t *readData;
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
 * @param index index of spi channel (number of channels defined by SPI_CS_PINS in sysConfig.hpp)
 * @param writeLength length of data to write
 * @param writeData data to write, must be in ram, not in flash
 * @param readLength length of data to read
 * @param readData data to read
 * @return use co_await on return value to await end of transfer
 */
[[nodiscard]] Awaitable<Parameters> transfer(int index, int writeLength, uint8_t const *writeData, int readLength, uint8_t *readData);

} // namespace spi
