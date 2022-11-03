#pragma once

#include <enum.hpp>
#include <Coroutine.hpp>
#include <cstdint>


/**
 * Support for digital inputs with simple debounce filter for mechanical buttons and switches
 * Note that an input and output can be mapped to the same pin in sysConfig.hpp
 */
namespace Input {

// Internal helper: Stores the parameters in the awaitable during co_await
struct Parameters {
	uint32_t risingFlags;
	uint32_t fallingFlags;
	int &index;
	bool &value;
};

/**
 * Initialize the digital inputs
 */
void init();

/**
 * READ an input directly without debounce filter
 * @param index index of input
 */
bool read(int index);

/**
 * Wait for a trigger event on multiple inputs. When a trigger occurs all coroutines are resumed that wait for it.
 * For example to wait on rising edge on input 0 and falling edge on input 2: co_await trigger(1 << 0, 1 << 2, index, value);
 * @param risingFlags for each input a flag to indicate that we want to wait for a rising edge
 * @param fallingFlags for each input a flag to indicate that we want to wait for a falling edge
 * @param index index of the input that actually triggered the event
 * @param value value of the input after the trigger occurred
 * @return use co_await on return value to await a trigger
 */
[[nodiscard]] Awaitable<Parameters> trigger(uint32_t risingFlags, uint32_t fallingFlags, int &index, bool &value);

} // namespace Input
