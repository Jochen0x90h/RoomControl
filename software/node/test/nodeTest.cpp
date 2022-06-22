#include "Message.hpp"
#include <bus.hpp>
#include <gtest/gtest.h>



TEST(nodeTest, getTypeLabel) {
	EXPECT_EQ(getTypeLabel(bus::EndpointType::BINARY_BUTTON), "Button");
}

/*
inline bool isBinaryOneShot(MessageType type) {
	return (type & MessageType::SWITCH_BINARY_CATEGORY) == bus::EndpointType::SWITCH_BINARY_ONESHOT;
}

inline bool isTernaryOneShot(MessageType type) {
	return (type & MessageType::SWITCH_TERNARY_CATEGORY) == bus::EndpointType::SWITCH_TERNARY_ONESHOT;
}
*/

TEST(nodeTest, isCompatible) {
	// up/downcast
	EXPECT_TRUE(isCompatible(bus::EndpointType::METERING_IN, bus::EndpointType::METERING_ELECTRIC_SUPPLY_PEAK_OUT));
	EXPECT_FALSE(isCompatible(bus::EndpointType::METERING_ELECTRIC_SUPPLY_PEAK_IN, bus::EndpointType::METERING_OUT));

	// compatible temperature <- temperature
	EXPECT_TRUE(isCompatible(bus::EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OUTDOOR_IN, bus::EndpointType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT));
	EXPECT_FALSE(isCompatible(bus::EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_IN, bus::EndpointType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT));

	// value <- switch
	EXPECT_TRUE(isCompatible(bus::EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_CMD_IN, bus::EndpointType::BINARY_BUTTON_OUT));
	EXPECT_TRUE(isCompatible(bus::EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER_CMD_IN, bus::EndpointType::BINARY_BUTTON_OUT));

	// switch <- value
	EXPECT_TRUE(isCompatible(bus::EndpointType::BINARY_IN, bus::EndpointType::PHYSICAL_TEMPERATURE_MEASURED_OUT));
	EXPECT_TRUE(isCompatible(bus::EndpointType::BINARY_IN, bus::EndpointType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT));
/*
	// binary one-shot
	EXPECT_TRUE(isBinaryOneShot(bus::EndpointType::SWITCH_BINARY_ONESHOT_BUTTON_OUT));
	EXPECT_FALSE(isBinaryOneShot(bus::EndpointType::SWITCH_BINARY_ON_OFF_IN));

	// ternary one-shot
	EXPECT_TRUE(isTernaryOneShot(bus::EndpointType::SWITCH_TERNARY_ONESHOT_ROCKER_IN));
	EXPECT_FALSE(isTernaryOneShot(bus::EndpointType::SWITCH_TERNARY_OPEN_CLOSE_OUT));
*/
	// float command <- float
	EXPECT_TRUE(isCompatible(bus::EndpointType::LIGHTING_BRIGHTNESS_CMD_IN, bus::EndpointType::LIGHTING_BRIGHTNESS_OUT));

	// float <- float command
	EXPECT_FALSE(isCompatible(bus::EndpointType::LIGHTING_BRIGHTNESS_IN, bus::EndpointType::LIGHTING_BRIGHTNESS_CMD_OUT));

	// switch <- float
	EXPECT_TRUE(isCompatible(bus::EndpointType::BINARY_POWER_LIGHT_IN, bus::EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_OUT));

	// switch <- float command
	EXPECT_FALSE(isCompatible(bus::EndpointType::BINARY_POWER_LIGHT_IN, bus::EndpointType::PHYSICAL_TEMPERATURE_SETPOINT_CMD_OUT));
}
