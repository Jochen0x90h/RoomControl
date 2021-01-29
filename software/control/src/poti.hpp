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
 * Set callback for digital potentiometer that notifies differential change
 * @param onChanged called when the quadrature decoder detected a change
 */
void setPotiHandler(std::function<void (int)> onChanged);

/**
 * Set callback for button activation
 * @param onActivated called when the button was activated
 */
void setButtonHandler(std::function<void ()> onActivated);

} // namespace poti
