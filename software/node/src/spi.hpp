#pragma once

#include <config.hpp>
#include <cstdint>
#include <functional>


// display, air sensor, fe-ram
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
	std::function<void ()> onTransferred);

#ifdef HAVE_SPI_DISPLAY
/**
 * Write to display that is connected to the SPI bus
 * @param data command and data to write, must be in ram, not in flash
 * @param commandLength length of command to write (may be 0)
 * @param dataLength length of data to write (may be 0)
 * @param onWritten completion handler 
 * @return true on success, false if busy with previous write
 */
bool writeDisplay(uint8_t const *data, int commandLength, int dataLength, std::function<void ()> onWritten);
#endif

} // namespace spi
