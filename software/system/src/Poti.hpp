#pragma once

#include <Coroutine.hpp>
#include <stdint.h>


/**
 * Support for a digital potentiometer
 */
namespace Poti {

// Internal helper: Stores references to the result values in the awaitable during co_await
struct Parameters {
	int8_t &delta;
};

/**
 * Initialize the digital potentiometer
 */
void init();

/**
 * Wait for a change of the digital potentiometer
 * @param index index of poti
 * @param delta delta motion of digital potentiometer
 * @param activated true when the button was pressed
 * @return use co_await on return value to await a change of the poti
 */
[[nodiscard]] Awaitable<Parameters> change(int index, int8_t& delta);

} // namespace Poti
