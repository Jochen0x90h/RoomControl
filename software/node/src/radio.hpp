#pragma once

#include <cstdint>
#include <functional>


namespace radio {

/**
 * Initialze the system
 */
void init();

/**
 * Handle radio events
 */
void handle();

/**
 * Set radio channel
 */
void setRadioChannel(int channel);

void sendRadio(uint8_t const* data, int length, std::function<void ()> onSent);

void receiveRadio(uint8_t* data, int length, std::function<void ()> onReceived);

} // namespace radio
