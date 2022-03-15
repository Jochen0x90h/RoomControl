#include "../Output.hpp"
#include "gpio.hpp"
#include <util.hpp>
#include <boardConfig.hpp>


/*
	Dependencies:
	
	Config:
		OUTPUTS: Array of OutputConfig
		OUTPUT_COUNT: Number of outputs
		
	Resources:
		GPIO
*/
namespace Output {

void init() {
	// configure outputs
	for (auto &output : OUTPUTS) {
		addOutputConfig(output);
	}
}

void enable(int index, bool enabled) {
	if (uint32_t(index) < OUTPUT_COUNT) {
		auto &output = OUTPUTS[index];
		enableOutput(output.index, enabled);	
	}
}

void set(int index, bool value) {
	if (uint32_t(index) < OUTPUT_COUNT) {
		auto &output = OUTPUTS[index];
		setOutput(output.index, value != output.invert);
	}
}

void toggle(int index) {
	if (uint32_t(index) < OUTPUT_COUNT) {
		auto &output = OUTPUTS[index];
		toggleOutput(output.index);
	}
}

} // namespace Output
