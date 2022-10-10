#pragma once

#include <Coroutine.hpp>
#include <stdint.h>


/**
 * Support for incremental encoders such as digital potentiometers
 */
namespace QuadratureDecoder {

// Internal helper: Stores references to the result values in the awaitable during co_await
struct Parameters {
	int8_t &delta;
};

/**
 * Initialize the incremental encoder
 */
void init();

/**
 * Wait for a change of the incremental encoder
 * @param index index of encoder
 * @param delta delta motion of incremental encoder
 * @return use co_await on return value to await a change of the encoder
 */
[[nodiscard]] Awaitable<Parameters> change(int index, int8_t& delta);

} // namespace QuadratureDecoder
