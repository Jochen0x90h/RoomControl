#pragma once

#include <enum.hpp>
#include <cstdint>


/**
 * cluster library (zcl)
 */
namespace zcl {

enum class Profile : uint16_t {
	HOME_AUTOMATION = 0x0104,
	GREEN_POWER = 0xa1e0,
	ZB_LIGHT_LINK = 0xc05e,
};

enum class Cluster : uint16_t {
	BASIC = 0x0000,
	POWER_CONFIGURATION = 0x0001,
	IDENTIFY = 0x0003,
	GROUPS = 0x0004,
	SCENES = 0x0005,
	ON_OFF = 0x0006,
	LEVEL_CONTROL = 0x0008,
	OTA_UPGRADE = 0x0019,
	GREEN_POWER = 0x0021,
	THERMOSTAT = 0x0201,
	COLOR_CONTROL = 0x0300,
	ZLL_COMMISSIONING = 0x1000,
};

enum class FrameControl : uint8_t {
	TYPE_MASK = 3,
	TYPE_PROFILE_WIDE = 0,
	TYPE_CLUSTER_SPECIFIC = 1,

	MANUFACTURER_SPECIFIC = 1 << 2,

	DIRECTION_MASK = 1 << 3,
	DIRECTION_SERVER_TO_CLIENT = 1 << 3,
	DIRECTION_CLIENT_TO_SERVER = 0,

	DISABLE_DEFAULT_RESPONSE = 1 << 4
};
FLAGS_ENUM(FrameControl)

// profile wide commands
enum class Command : uint8_t {
	READ_ATTRIBUTES = 0x00,
	READ_ATTRIBUTES_RESPONSE = 0x01,
	WRITE_ATTRIBUTES = 0x02,
	WRITE_ATTRIBUTES_UNDIVIDED = 0x03,
	WRITE_ATTRIBUTES_RESPONSE = 0x04,
	WRITE_ATTRIBUTES_NO_RESPONSE = 0x05,
	CONFIGURE_REPORTING = 0x06,
	CONFIGURE_REPORTING_RESPONSE = 0x07,
	READ_REPORTING_CONFIGURATION = 0x08,
	READ_REPORTING_CONFIGURATION_RESPONSE = 0x09,
	REPORT_ATTRIBUTES = 0x0a,
	DEFAULT_RESPONSE = 0x0b,
	DISCOVER_ATTRIBUTES = 0x0c,
	DISCOVER_ATTRIBUTES_RESPONSE = 0x0d
};

enum class Status : uint8_t {
	SUCCESS = 0x00,
	UNSUPPORTED_ATTRIBUTE = 0x86
};

enum class DataType : uint8_t  {
	BOOL = 0x10,

	UINT8 = 0x20,
	UINT16 = 0x21,
	UINT24 = 0x22,

	INT8 = 0x28,
	INT16 = 0x29,

	ENUM8 = 0x30,
	ENUM16 = 0x31,

	SEMI = 0x38, // half
	SINGLE = 0x39, // float
	DOUBLE = 0x3a,

	STRING = 0x42 // length in first byte
};

// basic cluster
// -------------

// attributes of basic cluster
enum class BasicAttribute : uint16_t {
	ZCL_VERSION = 0x0000, // (INT8U, required)
	APPLICATION_VERSION = 0x0001,
	STACK_VERSION = 0x0002,
	HW_VERSION = 0x0003,
	MANUFACTURER_NAME = 0x0004,
	MODEL_IDENTIFIER = 0x0005, // (CHAR_STRING)
	DATE_CODE = 0x0006,
	POWER_SOURCE = 0x0007, // (ENUM8, required)
	SOFTWARE_BUILD_ID = 0x4000,
	CLUSTER_REVISION = 0xFFFD, // (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

// types of attribute POWER_SOURCE of basic cluster
enum class BasicPowerSourceType : uint8_t {
	// mains (single phase)
	MAINS = 1,

	// battery
	BATTERY = 3
};

// power configuration cluster
// ---------------------------

// attributes of power configuration cluster
enum class PowerConfigurationAttribute : uint16_t {
	BATTERY_VOLTAGE = 0x0020, // (INT8U)
	BATTERY_PERCENTAGE = 0x0021, // (INT8U)
	REVISION = 0xFFFD, // cluster revision (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

// on off cluster
// --------------

// attributes of on off cluster
enum class OnOffAttribute : uint16_t {
	ON_OFF = 0x0000, // (BOOL, required)
	GLOBAL_SCENE_CONTROL = 0x4000, // (BOOL)
	ON_TIME = 0x4001, // INT16U
	OFF_WAIT_TIME = 0x4002, // INT16U
	CLUSTER_REVISION = 0xFFFD, // (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

enum class OnOffCommand : uint8_t {
	OFF = 0x00, // (required)
	ON = 0x01, // (required)
	TOGGLE = 0x02, // (required)
	OFF_WITH_EFFECT = 0x40,
	ON_WITH_RECALL_GLOBAL_SCENE = 0x41,
	ON_WITH_TIMED_OFF
};

// level control cluster
// ---------------------

enum class LevelControlAttribute : uint16_t {
	LEVEL = 0x0000, // current level (UINT8, read only, required)
	REMAINING_TIME = 0x0001,
	OPTIONS = 0x000F,
	ON_OFF_TRANSITION_TIME = 0x0010,
	ON_LEVEL = 0x0011,
	ON_TRANSITION_TIME = 0x0012,
	OFF_TRANSITION_TIME = 0x0013,
	DEFAULT_MOVE_RATE = 0x0014,
	START_UP_CURRENT_LEVEL = 0x4000,
	CLUSTER_REVISION = 0xFFFD, // (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

enum class LevelControlCommand : uint8_t {
	MOVE_TO_LEVEL = 0x00, // (required)
	MOVE = 0x01, // (required)
	STEP = 0x02, // (required)
	STOP = 0x03, // (required)
	MOVE_TO_LEVEL_WITH_ON_OFF = 0x04, // (required)
	MOVE_WITH_ON_OFF = 0x05, // (required)
	STEP_WITH_ON_OFF = 0x06, // (required)
	STOP_WITH_ON_OFF = 0x07, // (required)
};

// color control cluster
// ---------------------

enum class ColorControlAttribute : uint16_t {
	COLOR_X = 0x0003, // current color, x component of CIE xyY color model (INT16U / 65536, read only, required)
	COLOR_Y = 0x0004, // current color, y component of CIE xyY color model (INT16U / 65536, read only, required)
	OPTIONS = 0x000F, // options (BITMAP8, required)
	COUPLE_COLOR_TEMP_TO_LEVEL_MIN_MIREDS = 0x400D, // (INT16U, required)
	START_UP_COLOR_TEMP_MIREDS = 0x4010,
	CLUSTER_REVISION = 0xFFFD, // (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

enum class ColorControlCommand : uint8_t {
	MOVE_TO_COLOR = 0x07, // (INT16U, required)
	MOVE_COLOR = 0x08, // (required)
	STEP_COLOR = 0x09, // (INT16S, required)
	STOP_MOVE_STEP = 0x47
};

// thermostat
// ----------

enum class ThermostatAttribute : uint16_t {
	LOCAL_TEMPERATURE = 0x0000, // (INT16S / 100 Celsius, read only, required)
	OUTDOOR_TEMPERATURE = 0x0001, // (INT16S, read only)
	OCCUPIED_COOLING_SETPOINT = 0x0011, // (INT16S, required)
	OCCUPIED_HEATING_SETPOINT = 0x0012, // (INT16S, required)
	UNOCCUPIED_COOLING_SETPOINT = 0x0013, // (INT16S)
	UNOCCUPIED_HEATING_SETPOINT = 0x0014, // (INT16S)
	CONTROL_SEQUENCE_OF_OPERATION = 0x001B, // (ENUM8, required)
	SYSTEM_MODE = 0x001C, // (ENUM8, required)
	CLUSTER_REVISION = 0xFFFD, // (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

enum class ThermostatCommand : uint16_t {

};

} // namespace zcl
