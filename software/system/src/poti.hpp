#pragma once

#include <Coroutine.hpp>


namespace poti {

// Internal helper: Stores references to the result values in the awaitable during co_await
struct Parameters {
	int &delta;
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
Awaitable<Parameters> change(int& delta, bool& activated);

} // namespace poti
