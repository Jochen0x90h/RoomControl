#pragma once

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
 * Write to display
 * @param data command and data to write, must be in ram, not in flash
 * @param commandLength length of command to write (may be 0)
 * @param dataLength length of data to write (may be 0)
 * @param onWritten completion handler 
 * @return true on success, false if busy with previous write
 */
bool writeDisplay(uint8_t const *data, int commandLength, int dataLength, std::function<void ()> onWritten);

/**
 * Transfer data to/from air sensor
 * @param writeData data to write, must be in ram, not in flash
 * @param writeLength length of data to write
 * @param readData data to read
 * @param readLength length of data to read
 * @param onTransferred completion handler
 * @return true on success, false if busy with previous transfer
 */
bool transferAirSensor(uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> onTransferred);

/**
 * Transfer data to/from FeRam
 * @param writeData data to write, must be in ram, not in flash
 * @param writeLength length of data to write
 * @param readData data to read
 * @param readLength length of data to read
 * @param onTransferred completion handler
 * @return true on success, false if busy with previous transfer
 */
bool transferFeRam(uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> onTransferred);

} // namespace spi
