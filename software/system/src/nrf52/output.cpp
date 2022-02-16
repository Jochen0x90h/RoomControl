#include "../output.hpp"
#include "gpio.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


namespace output {

void init() {
	for (auto &output : OUTPUTS) {
		// set to initial value
		setOutput(output.pin, output.initialValue != output.invert);
		
		// configure as output
		addOutputConfig(output.pin, output.drive, output.pull, output.enabled);
	}
}

void enable(int index, bool enabled) {
	if (uint32_t(index) < OUTPUT_COUNT) {
		auto &output = OUTPUTS[index];
		enableOutput(output.pin, enabled);	
	}
}

void set(int index, bool value) {
	if (uint32_t(index) < OUTPUT_COUNT) {
		auto &output = OUTPUTS[index];
		setOutput(output.pin, value != output.invert);
	}
}

void toggle(int index) {
	if (uint32_t(index) < OUTPUT_COUNT) {
		auto &output = OUTPUTS[index];
		toggleOutput(output.pin);
	}
}

} // namespace output
