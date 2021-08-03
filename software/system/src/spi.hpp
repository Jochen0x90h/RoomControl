#pragma once

#include <cstdint>
#include <functional>
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
void init();

/**
 * Suspend execution using co_await until transfer of data to/from SPI device is finished
 * @param index index of spi channel (number of channels defined by SPI_CS_PINS in sysConfig.hpp)
 * @param writeData data to write, must be in ram, not in flash
 * @param writeLength length of data to write
 * @param readData data to read
 * @param readLength length of data to read
 */
Awaitable<Parameters> transfer(int index, int writeLength, uint8_t const *writeData, int readLength, uint8_t *readData);

} // namespace spi
