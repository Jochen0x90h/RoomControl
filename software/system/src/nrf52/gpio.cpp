#include "../gpio.hpp"
#include "global.hpp"
#include <util.hpp>


namespace gpio {

void init() {
	for (auto config : OUT_CONFIGS) {
		// set off
		setOutput(config.pin, !config.on);
		
		// set output mode
		configureOutput(config.pin);	
	}
}

void set(int index, bool value) {
	if (uint32_t(index) < array::count(OUT_CONFIGS)) {
		auto config = OUT_CONFIGS[index];
		setOutput(config.pin, value == config.on);
	}
}

void toggle(int index) {
	if (uint32_t(index) < array::count(OUT_CONFIGS)) {
		auto config = OUT_CONFIGS[index];
		toggleOutput(config.pin);
	}
}

} // namespace gpio
