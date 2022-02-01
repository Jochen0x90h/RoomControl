#pragma once

#include <Coroutine.hpp>
#include <stdint.h>


/**
 * Support for a digital potentiometer with push button
 */
namespace poti {

// Internal helper: Stores references to the result values in the awaitable during co_await
struct Parameters {
	int8_t &delta;
	bool &activated;
};

/**
 * Initialize the digital potentiometer with button. Depends on timer, also call timer::init()
 */
void init();

/**
 * Suspend execution using co_await until the digital potentiometer or button changed. Only one coroutine can wait.
 * @param delta delta motion of digital potentiometer
 * @param activated true when the button was pressed
 */
[[nodiscard]] Awaitable<Parameters> change(int8_t& delta, bool& activated);

} // namespace poti
