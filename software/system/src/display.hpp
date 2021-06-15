#pragma once

#include <Coroutine.hpp>
#include <cstdint>
#include <functional>


namespace display {

struct Parameters {
	int commandLength;
	int dataLength;
	uint8_t const *data;
};


/**
 * Initialize display. Depends on SPI, also call spi::init()
 */
void init();

/**
 * Enable Vcc of display
 */
inline void enableVcc(bool enable) {}

/**
 * Suspend execution using co_await until sending of data to display is finished
 * @param commandLength length of command to write (may be 0)
 * @param dataLength length of data to write (may be 0)
 * @param data command and/or data to write, must be in ram, not in flash
 */
Awaitable<Parameters> send(int commandLength, int dataLength, uint8_t const *data);

} // namespace display
