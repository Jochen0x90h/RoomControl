#pragma once

#include <functional>


namespace poti {

/**
 * Initialze the digital potentiometer
 */
void init();

/**
 * Handle the digital potentiometer
 */
void handle();

/**
 * Set callback for digital potentiometer that notifies differential change or button press
 * @param onChanged called when the quadrature decoder detected a change or button was activated
 */
void setHandler(std::function<void (int, bool)> onChanged);

} // namespace poti
