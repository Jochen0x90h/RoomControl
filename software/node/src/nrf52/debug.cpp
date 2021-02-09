#include "../debug.hpp"
#include "global.hpp"


namespace debug {

void init() {
	setOutput(LED_RED_PIN, true);
	configureOutput(LED_RED_PIN);
	setOutput(LED_GREEN_PIN, true);
	configureOutput(LED_GREEN_PIN);
	setOutput(LED_BLUE_PIN, true);
	configureOutput(LED_BLUE_PIN);	
}

void setRedLed(bool value) {
	setOutput(LED_RED_PIN, !value);
}

void setGreenLed(bool value) {
	setOutput(LED_GREEN_PIN, !value);
}

void setBlueLed(bool value) {
	setOutput(LED_BLUE_PIN, !value);
}

} // namespace debug
