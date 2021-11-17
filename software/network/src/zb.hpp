#pragma once

#include <enum.hpp>


namespace zb {

// network layer frame control field
enum class NwkFrameControl : uint16_t {
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
	
	DISCOVER_ROUTE_MASK = 3 << 6,
	DISCOVER_ROUTE_SUPPRESS = 0 << 6,
	DISCOVER_ROUTE_ENABLE = 1 << 6,
	
	MULTICAST = 1 << 8,

	SECURITY = 1 << 9,

	SOURCE_ROUTE = 1 << 10,

	DESTINATION = 1 << 11,

	EXTENDED_SOURCE = 1 << 12,

	END_DEVICE_INITIATOR = 1 << 13
};
FLAGS_ENUM(NwkFrameControl)

// network layer command (reference chapter 3.4)
enum class NwkCommand : uint8_t {
	ROUTE_REQUEST = 1,
	ROUTE_REPLY = 2,
	NETWORK_STATUS = 3,
	LEAVE = 4,
	ROUTE_RECORD = 5,
	REJOIN_REQUEST = 6,
	REJOIN_RESPONSE = 7,
	LINK_STATUS = 8,
	NETWORK_REPORT = 9,
	NETWORK_UPDATE = 10
};

enum class NwkCommandRouteRequestOptions : uint8_t {
	DISCOVERY_MASK = 3 << 3,
	DISCOVERY_SINGLE = 0,
	DISCOVERY_MANY_TO_ONE_WITH_SOURCE_ROUTING = 1 << 3,
	
	EXTENDED_DESTINATION = 1 << 5,
	
	MULTICAST = 1 << 6
};
FLAGS_ENUM(NwkCommandRouteRequestOptions)

enum class NwkCommandRouteReplyOptions : uint8_t {
	EXTENDED_ORIGINATOR = 1 << 4,

	EXTENDED_RESPONDER = 1 << 5,
	
	MULTICAST = 1 << 6
};
FLAGS_ENUM(NwkCommandRouteReplyOptions)

enum class NwkCommandRejoinRequestOptions : uint8_t {
	ALTERNATE_PAN_COORDINATOR = 1,

	FULL_FUNCTION_DEVICE = 1 << 1,

	AC_POWER = 1 << 2,

	RECEIVE_ON_WHEN_IDLE = 1 << 3,

	SECURITY_CAPABILITY = 1 << 6,

	ALLOCATE_SHORT_ADDRESS = 1 << 7
};
FLAGS_ENUM(NwkCommandRejoinRequestOptions)

enum class NwkCommandLinkStatusOptions : uint8_t {
	LINK_STATUS_COUNT_MASK = 31,
	FIRST_FRAME = 1 << 5,
	LAST_FRAME = 1 << 6
};
FLAGS_ENUM(NwkCommandLinkStatusOptions)


// application support layer (aps)
// -------------------------------

// application support layer frame control field
enum class ApsFrameControl : uint8_t {
	TYPE_MASK = 3,
	TYPE_DATA = 0,
	TYPE_COMMAND = 1,
	TYPE_ACK = 2,
	
	DELIVERY_MASK = 3 << 2,
	DELIVERY_UNICAST = 0 << 2,

	SECURITY = 1 << 5,
	
	ACKNOWLEDGEMENT_REQUEST = 1 << 6,
	
	EXTENDED = 1 << 7
};
FLAGS_ENUM(ApsFrameControl)

enum class ApsCommand : uint8_t  {
	TRANSPORT_KEY = 5,
	UPDATE_DEVICE = 6,
};


// device profile (zdp)
// --------------------

enum class ZdpCommand : uint16_t {
	NETWORK_ADDRESS_REQUEST = 0x0000,
	
	EXTENDED_ADDRESS_REQUEST = 0x0001,
	EXTENDED_ADDRESS_RESPONSE = 0x8001,

	NODE_DESCRIPTOR_REQUEST = 0x0002,
	NODE_DESCRIPTOR_RESPONSE = 0x8002,

	SIMPLE_DESCRIPTOR_REQUEST = 0x0004,
	SIMPLE_DESCRIPTOR_RESPONSE = 0x8004,

	ACTIVE_ENDPOINT_REQUEST = 0x0005,
	ACTIVE_ENDPOINT_RESPONSE = 0x8005,

	MATCH_DESCRIPTOR_REQUEST = 0x0006,
	MATCH_DESCRIPTOR_RESPONSE = 0x8006,

	DEVICE_ANNOUNCEMENT = 0x0013,

	BIND_REQUEST = 0x0021,
	BIND_RESPONSE = 0x8021,

	PERMIT_JOIN_REQUEST = 0x0036,
};


// cluster library (zcl)
// ---------------------

enum class ZclProfile : uint16_t {
	HOME_AUTOMATION = 0x104,
	GREEN_POWER = 0xa1e0,
	ZB_LIGHT_LINK = 0xc05e,
};

enum class ZclCluster : uint16_t {
	BASIC = 0x0000,
	POWER_CONFIGURATION = 0x0001,
	IDENTIFY = 0x0003,
	GROUPS = 0x0004,
	SCENES = 0x0005,
	ON_OFF = 0x0006,
	LEVEL_CONTROL = 0x0008,
	OTA_UPGRADE = 0x0019,
	GREEN_POWER = 0x0021,
	ZLL_COMMISSIONING = 0x1000,
};

enum class ZclFrameControl : uint8_t {
	TYPE_MASK = 3,
	TYPE_PROFILE_WIDE = 0,
	TYPE_CLUSTER_SPECIFIC = 1,
	
	MANUFACTURER_SPECIFIC = 1 << 2,
	
	DIRECTION_MASK = 1 << 3,
	DIRECTION_SERVER_TO_CLIENT = 1 << 3,
	DIRECTION_CLIENT_TO_SERVER = 0,
	
	DISABLE_DEFAULT_RESPONSE = 1 << 4
};
FLAGS_ENUM(ZclFrameControl)

// profile wide commands
enum class ZclCommand : uint8_t {
	READ_ATTRIBUTES = 0,
	READ_ATTRIBUTES_RESPONSE = 1,
	CONFIGURE_REPORTING = 6,
	CONFIGURE_REPORTING_RESPONSE = 7,
	DEFAULT_RESPONSE = 0x0b
};

enum class ZclStatus : uint8_t {
	SUCCESS = 0x00,
	UNSUPPORTED_ATTRIBUTE = 0x86
};

enum class ZclDataType : uint8_t  {
	UINT8 = 0x20,
	STRING = 0x42
};

// basic cluster
// -------------

// attributes of basic cluster
enum class ZclBasicAttribute : uint16_t {
	ZCL_VERSION = 0x0000,
	APPLICATION_VERSION = 0x0001,
	STACK_VERSION = 0x0002,
	HW_VERSION = 0x0003,
	MANUFACTURER_NAME = 0x0004,
	MODEL_IDENTIFIER = 0x0005,
	DATE_CODE = 0x0006,
	POWER_SOURCE = 0x0007,
	SOFTWARE_BUILD_ID = 0x4000
};

// types of attribute POWER_SOURCE of basic cluster
enum class ZclPowerSourceType : uint8_t {
	// mains (single phase)
	MAINS = 1,
	
	// battery
	BATTERY = 3
};

// power configuration cluster
// ---------------------------

// attributes of power configuration cluster
enum ZclPowerConfigurationAttribute : uint16_t {
	BATTERY_VOLTAGE = 0x0020,
	BATTERY_PERCENTAGE = 0x0021,
};

// on off cluster
// --------------

// attributes of on off cluster
enum ZclOnOffAttribute : uint16_t {
	ON_OFF = 0x0000,
	GLOBAL_SCENE_CONTROL = 0x4000,
	ON_TIME = 0x4001,
	OFF_WAIT_TIME = 0x4002
};

enum ZclOnOffCommand : uint8_t {
	OFF = 0x00,
	ON = 0x01,
	TOGGLE = 0x02,
	OFF_WITH_EFFECT = 0x40,
	ON_WITH_RECALL_GLOBAL_SCENE = 0x41
};

// level control cluster
// ---------------------

// attributes of level control cluster
enum ZclLevelControlAttribute : uint16_t {
	CURRENT_LEVEL = 0x0000,
	REMAINING_TIME = 0x0001,
	ON_OFF_TRANSITION_TIME = 0x0010,
	ON_LEVEL = 0x0011,
	ON_TRANSITION_TIME = 0x0012,
	OFF_TRANSITION_TIME = 0x0013,
	DEFAULT_MOVE_RATE = 0x0014
};

enum ZclLevelControlCommand : uint8_t {
	MOVE_TO_LEVEL = 0x00,
	MOVE = 0x01,
	STEP = 0x02,
	STOP = 0x03,
	MOVE_TO_LEVEL_WITH_ON_OFF = 0x04,
	MOVE_WITH_ON_OFF = 0x05,
	STEP_WITH_ON_OFF = 0x06,
	STOP_WITH_ON_OFF = 0x07,
};

// security
// --------

// key type (table 4.31)
enum class KeyIdentifier : uint8_t  {
	DATA = 0,
	NETWORK = 1,
	KEY_TRANSPORT = 2,
	KEY_LOAD = 3
};

// security control field (4.5.1.1)
enum class SecurityControl : uint8_t {
	// security level
	LEVEL_MASK = 7,
	LEVEL_NONE = 0,
	LEVEL_MIC32 = 1,
	LEVEL_MIC64 = 2,
	LEVEL_MIC128 = 3,
	LEVEL_ENC = 4,
	LEVEL_ENC_MIC32 = 5, // default
	LEVEL_ENC_MIC64 = 6,
	LEVEL_ENC_MIC128 = 7,

	// key type (table 4.31)
	KEY_MASK = 3 << 3,
	KEY_LINK = 0,
	KEY_NETWORK = 1 << 3,
	KEY_KEY_TRANSPORT = 2 << 3,
	KEY_KEY_LOAD = 3 << 3,

	EXTENDED_NONCE = 1 << 5,
};
FLAGS_ENUM(SecurityControl)

} // namespace zb
