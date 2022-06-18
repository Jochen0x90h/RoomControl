#include "Message.hpp"
#include <bus.hpp>
#include <gtest/gtest.h>



TEST(nodeTest, getTypeLabel) {
	EXPECT_EQ(getTypeLabel(bus::EndpointType2::SWITCH_BINARY_ONESHOT_BUTTON), "Button");
}

TEST(nodeTest, isCompatible) {
	// up/downcast
	EXPECT_TRUE(isCompatible(bus::EndpointType2::METERING_IN, bus::EndpointType2::METERING_ELECTRIC_SUPPLY_PEAK_OUT));
	EXPECT_FALSE(isCompatible(bus::EndpointType2::METERING_ELECTRIC_SUPPLY_PEAK_IN, bus::EndpointType2::METERING_OUT));

	// temperature <- temperature
	EXPECT_TRUE(isCompatible(bus::EndpointType2::PHYSICAL_TEMPERATURE_MEASURED_ENVIRONMENT_IN, bus::EndpointType2::PHYSICAL_TEMPERATURE_MEASURED_LOCAL_OUT));
	EXPECT_FALSE(isCompatible(bus::EndpointType2::PHYSICAL_TEMPERATURE_SETPOINT_IN, bus::EndpointType2::PHYSICAL_TEMPERATURE_MEASURED_LOCAL_OUT));

	// value <- switch
	EXPECT_TRUE(isCompatible(bus::EndpointType2::PHYSICAL_TEMPERATURE_SETPOINT_IN, bus::EndpointType2::SWITCH_BINARY_ONESHOT_BUTTON_OUT));
	EXPECT_TRUE(isCompatible(bus::EndpointType2::PHYSICAL_TEMPERATURE_SETPOINT_MIN_IN, bus::EndpointType2::SWITCH_BINARY_ONESHOT_BUTTON_OUT));

	// switch -> value
	EXPECT_TRUE(isCompatible(bus::EndpointType2::SWITCH_BINARY_ON_OFF_IN, bus::EndpointType2::PHYSICAL_TEMPERATURE_MEASURED_OUT));
	EXPECT_TRUE(isCompatible(bus::EndpointType2::SWITCH_BINARY_ON_OFF_IN, bus::EndpointType2::PHYSICAL_TEMPERATURE_MEASURED_ENVIRONMENT_OUT));

	// binary one-shot
	EXPECT_TRUE(isBinaryOneShot(bus::EndpointType2::SWITCH_BINARY_ONESHOT_BUTTON_OUT));
	EXPECT_FALSE(isBinaryOneShot(bus::EndpointType2::SWITCH_BINARY_ON_OFF_IN));

	// ternary one-shot
	EXPECT_TRUE(isTernaryOneShot(bus::EndpointType2::SWITCH_TERNARY_ONESHOT_ROCKER_IN));
	EXPECT_FALSE(isTernaryOneShot(bus::EndpointType2::SWITCH_TERNARY_OPEN_CLOSE_OUT));

	// float command <- float
	EXPECT_TRUE(isCompatible(bus::EndpointType2::LIGHTING_BRIGHTNESS_CMD_IN, bus::EndpointType2::LIGHTING_BRIGHTNESS_OUT));

	// float <- float command
	EXPECT_FALSE(isCompatible(bus::EndpointType2::LIGHTING_BRIGHTNESS_IN, bus::EndpointType2::LIGHTING_BRIGHTNESS_CMD_OUT));

	// switch <- float
	EXPECT_TRUE(isCompatible(bus::EndpointType2::SWITCH_BINARY_ON_OFF_IN, bus::EndpointType2::PHYSICAL_TEMPERATURE_SETPOINT_OUT));

	// switch <- float command
	EXPECT_FALSE(isCompatible(bus::EndpointType2::SWITCH_BINARY_ON_OFF_IN, bus::EndpointType2::PHYSICAL_TEMPERATURE_SETPOINT_CMD_OUT));
}
