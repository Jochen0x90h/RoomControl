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
void toggleRedLed() {
	toggleOutput(LED_RED_PIN);
}

void setGreenLed(bool value) {
	setOutput(LED_GREEN_PIN, !value);
}
void toggleGreenLed() {
	toggleOutput(LED_GREEN_PIN);
}

void setBlueLed(bool value) {
	setOutput(LED_BLUE_PIN, !value);
}
void toggleBlueLed() {
	toggleOutput(LED_BLUE_PIN);
}

} // namespace debug
