#pragma once

#include <enum.hpp>


namespace gp {

// network layer frame control field
enum class NwkFrameControl : uint8_t {
	// frame type
	TYPE_MASK = 3,
	TYPE_DATA = 0,
	TYPE_COMMAND = 1,

	// protocol version
	VERSION_MASK = 15 << 2,
	VERSION_1 = 1 << 2,
	// https://zigbeealliance.org/wp-content/uploads/2019/12/docs-05-3474-21-0csg-zigbee-specification.pdf
	// https://zigbeealliance.org/wp-content/uploads/2019/12/07-5123-06-zigbee-cluster-library-specification.pdf
	VERSION_2 = 2 << 2,
	// https://zigbeealliance.org/wp-content/uploads/2019/11/docs-09-5499-26-batt-zigbee-green-power-specification.pdf
	VERSION_3_GP = 3 << 2,
	
	AUTO_COMMISSIONING = 1 << 6,
	
	EXTENDED = 1 << 7
};
FLAGS_ENUM(NwkFrameControl)

// network layer extended frame control field
enum class NwkExtendedFrameControl : uint8_t {
	NONE = 0,
	
	APPLICATION_ID_MASK = 7,
	
	SECURITY_LEVEL_MASK = 3 << 3,
	SECURITY_LEVEL_NONE = 0,
	SECURITY_LEVEL_CNT8_MIC16 = 1 << 3,
	SECURITY_LEVEL_CNT32_MIC32 = 2 << 3,
	SECURITY_LEVEL_ENC_CNT32_MIC32 = 3 << 3,
	
	SECURITY_KEY = 1 << 5,
	
	SECURITY_RX_AFTER_TX = 1 << 6,
	
	DIRECTION_MASK = 1 << 7,
	DIRECTION_FROM_ZGPD = 0,
	DIRECTION_TO_ZGPD = 1 << 7
};
FLAGS_ENUM(NwkExtendedFrameControl)

enum class DeviceType : uint8_t {
	ON_OFF_SWITCH = 0x02,


	GENERIC_SWITCH = 0x07
};

enum class Command : uint8_t {
	SCENE0 = 0x10,
	SCENE1 = 0x11,
	SCENE2 = 0x12,
	SCENE3 = 0x13,
	SCENE4 = 0x14,
	SCENE5 = 0x15,
	COMMISSIONING = 0xe0,
};


// commissioning options
enum class Options : uint8_t {
	NONE = 0,
	
	MAC_SEQUENCE_NUMBER = 1,
	
	RX_ON_CAPABILITY = 1 << 1,
	
	APPLICATION_INFORMATION = 1 << 2,
	
	PAN_ID_REQUEST = 1 << 4,
	
	GP_SECURITY_KEY_REQUEST = 1 << 5,
	
	FIXED_LOCATION = 1 << 6,
	
	EXTENDED = 1 << 7	
};
FLAGS_ENUM(Options)

enum class ExtendedOptions : uint8_t {
	NONE = 0,
	
	SECURITY_LEVEL_CAPABILITY_MASK = 3,
	SECURITY_LEVEL_CAPABILITY_NONE = 0,
	SECURITY_LEVEL_CAPABILITY_CNT8_MIC16 = 1,
	SECURITY_LEVEL_CAPABILITY_CNT32_MIC32 = 2,
	SECURITY_LEVEL_CAPABILITY_ENC_CNT32_MIC32 = 3,
	
	KEY_TYPE_MASK = 7 << 2,
	KEY_TYPE_INDIVIDUAL = 4 << 2,
	
	KEY_PRESENT = 1 << 5,

	KEY_ENCRYPTED = 1 << 6,
	
	COUNTER_PRESENT = 1 << 7
};
FLAGS_ENUM(ExtendedOptions)

} // namespace gp
