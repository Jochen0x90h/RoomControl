#pragma once

#include <cstdint>
#include <functional>


namespace display {

/**
 * Initialze display. Depends on SPI, also call spi::init()
 */
void init();

/**
 * Enable Vcc of display
 */
inline void enableVcc(bool enable) {}

/**
 * Send to display that is connected to the SPI bus
 * @param data command and data to write, must be in ram, not in flash
 * @param commandLength length of command to write (may be 0)
 * @param dataLength length of data to write (may be 0)
 * @param onSent completion handler 
 * @return true on success, false if busy with previous send
 */
bool send(uint8_t const *data, int commandLength, int dataLength, std::function<void ()> const &onSent);

} // namespace display
