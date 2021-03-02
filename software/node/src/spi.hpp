#pragma once

#include <config.hpp>
#include <cstdint>
#include <functional>


namespace spi {

/**
 * Initialze SPI
 */
void init();

/**
 * Handle SPI events
 */
void handle();

/**
 * Transfer data to/from SPI device
 * @param csPin pin that is connected to the chip select pin of the device
 * @param writeData data to write, must be in ram, not in flash
 * @param writeLength length of data to write
 * @param readData data to read
 * @param readLength length of data to read
 * @param onTransferred completion handler
 * @return true on success, false if busy with previous transfer
 */
bool transfer(int csPin, uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> const &onTransferred);

} // namespace spi
