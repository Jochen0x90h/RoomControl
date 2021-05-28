#pragma once

#include <cstdint>
#include <functional>


namespace spi {

/**
 * Initialize SPI
 */
void init();

/**
 * Transfer data to/from SPI device
 * @param index index of spi channel (number of channels defined by SPI_CS_PINS in sysConfig.hpp)
 * @param writeData data to write, must be in ram, not in flash
 * @param writeLength length of data to write
 * @param readData data to read
 * @param readLength length of data to read
 * @param onTransferred completion handler
 * @return true on success, false if busy with previous transfer
 */
bool transfer(int index, uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> const &onTransferred);

} // namespace spi
