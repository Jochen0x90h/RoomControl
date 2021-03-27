#pragma once

#include <functional>


namespace poti {

/**
 * Initialze the digital potentiometer
 */
void init();

/**
 * Set callback for digital potentiometer that notifies differential change or button press
 * @param onChanged called when the quadrature decoder detected a change or button was activated
 */
void setHandler(std::function<void (int, bool)> const &onChanged);

} // namespace poti
