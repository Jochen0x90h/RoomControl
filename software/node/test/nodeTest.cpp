#include "Message.hpp"
#include <bus.hpp>
#include <gtest/gtest.h>



TEST(nodeTest, getTypeLabel) {
	EXPECT_EQ(getTypeLabel(bus::PlugType::BINARY_BUTTON), "Button");
}

/*
inline bool isBinaryOneShot(MessageType type) {
	return (type & MessageType::SWITCH_BINARY_CATEGORY) == bus::PlugType::SWITCH_BINARY_ONESHOT;
}

inline bool isTernaryOneShot(MessageType type) {
	return (type & MessageType::SWITCH_TERNARY_CATEGORY) == bus::PlugType::SWITCH_TERNARY_ONESHOT;
}
*/

TEST(nodeTest, isCompatible) {
	// up/downcast
	EXPECT_TRUE(isCompatible(bus::PlugType::METERING_IN, bus::PlugType::METERING_ELECTRIC_SUPPLY_PEAK_OUT));
	EXPECT_FALSE(isCompatible(bus::PlugType::METERING_ELECTRIC_SUPPLY_PEAK_IN, bus::PlugType::METERING_OUT));

	// compatible temperature <- temperature
	EXPECT_TRUE(isCompatible(bus::PlugType::PHYSICAL_TEMPERATURE_MEASURED_OUTDOOR_IN, bus::PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT));
	EXPECT_FALSE(isCompatible(bus::PlugType::PHYSICAL_TEMPERATURE_SETPOINT_IN, bus::PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT));

	// value <- switch
	EXPECT_TRUE(isCompatible(bus::PlugType::PHYSICAL_TEMPERATURE_SETPOINT_CMD_IN, bus::PlugType::BINARY_BUTTON_OUT));
	EXPECT_TRUE(isCompatible(bus::PlugType::PHYSICAL_TEMPERATURE_SETPOINT_HEATER_CMD_IN, bus::PlugType::BINARY_BUTTON_OUT));

	// switch <- value
	EXPECT_TRUE(isCompatible(bus::PlugType::BINARY_IN, bus::PlugType::PHYSICAL_TEMPERATURE_MEASURED_OUT));
	EXPECT_TRUE(isCompatible(bus::PlugType::BINARY_IN, bus::PlugType::PHYSICAL_TEMPERATURE_MEASURED_ROOM_OUT));
/*
	// binary one-shot
	EXPECT_TRUE(isBinaryOneShot(bus::PlugType::SWITCH_BINARY_ONESHOT_BUTTON_OUT));
	EXPECT_FALSE(isBinaryOneShot(bus::PlugType::SWITCH_BINARY_ON_OFF_IN));

	// ternary one-shot
	EXPECT_TRUE(isTernaryOneShot(bus::PlugType::SWITCH_TERNARY_ONESHOT_ROCKER_IN));
	EXPECT_FALSE(isTernaryOneShot(bus::PlugType::SWITCH_TERNARY_OPEN_CLOSE_OUT));
*/
	// float command <- float
	EXPECT_TRUE(isCompatible(bus::PlugType::LIGHTING_BRIGHTNESS_CMD_IN, bus::PlugType::LIGHTING_BRIGHTNESS_OUT));

	// float <- float command
	EXPECT_FALSE(isCompatible(bus::PlugType::LIGHTING_BRIGHTNESS_IN, bus::PlugType::LIGHTING_BRIGHTNESS_CMD_OUT));

	// switch <- float
	EXPECT_TRUE(isCompatible(bus::PlugType::BINARY_POWER_LIGHT_IN, bus::PlugType::PHYSICAL_TEMPERATURE_SETPOINT_OUT));

	// switch <- float command
	EXPECT_FALSE(isCompatible(bus::PlugType::BINARY_POWER_LIGHT_IN, bus::PlugType::PHYSICAL_TEMPERATURE_SETPOINT_CMD_OUT));
}
