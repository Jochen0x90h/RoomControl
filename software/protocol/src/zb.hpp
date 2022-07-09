#pragma once

#include "crypt.hpp"
#include <enum.hpp>


namespace zb {

// zbee alliance 2009 key
extern uint8_t const za09LinkKey[16];
extern AesKey const za09LinkAesKey;
extern AesKey const za09KeyTransportAesKey;
extern AesKey const za09KeyLoadAesKey;

// zbee light link key
extern uint8_t const zllLinkKey[16];


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
	DISCOVERY_SINGLE = 0, // "Not Many-to-One" in Wireshark
	DISCOVERY_MANY_TO_ONE_WITH_SOURCE_ROUTING = 1 << 3,

	EXTENDED_DESTINATION = 1 << 5,

	MULTICAST = 1 << 6
};
FLAGS_ENUM(NwkCommandRouteRequestOptions)

enum class NwkCommandRouteReplyOptions : uint8_t {
	EXTENDED_ORIGINATOR = 1 << 4,

	EXTENDED_DESTINATION = 1 << 5, // "Responder" in Wireshark

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
	NONE = 0,

	TYPE_MASK = 3,
	TYPE_DATA = 0,
	TYPE_COMMAND = 1,
	TYPE_ACK = 2,

	DELIVERY_MASK = 3 << 2,
	DELIVERY_UNICAST = 0 << 2,

	ACK_FORMAT = 1 << 4,

	SECURITY = 1 << 5,

	ACK_REQUEST = 1 << 6,

	EXTENDED = 1 << 7
};
FLAGS_ENUM(ApsFrameControl)

enum class ApsCommand : uint8_t  {
	TRANSPORT_KEY = 0x5,
	UPDATE_DEVICE = 0x6,
	REQUEST_KEY = 0x8,
	VERIFY_KEY = 0x0f,
	CONFIRM_KEY = 0x10
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

	RESPONSE_FLAG = 0x8000
};
FLAGS_ENUM(ZdpCommand)

enum class ZdpNodeDescriptorInfo : uint16_t {
	TYPE_MASK = 0x0007,
	TYPE_COORDINATOR = 0,

	BAND_868MHZ = 1 << 11,
	BAND_902MHZ = 1 << 13,
	BAND_2_4GHZ = 1 << 14,
	BAND_EU_SUB_GHZ = 1 << 15
};
FLAGS_ENUM(ZdpNodeDescriptorInfo)

// for capabilities use ieee::DeviceInfo

enum class ZdpNodeDescriptorServerFlags : uint16_t {
	PRIMARY_TRUST_CENTER = 1,
	REVISION_22 = 22 << 9
};
FLAGS_ENUM(ZdpNodeDescriptorServerFlags)


// cluster library (zcl)
// ---------------------

enum class ZclProfile : uint16_t {
	HOME_AUTOMATION = 0x0104,
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
	THERMOSTAT = 0x0201,
	COLOR_CONTROL = 0x0300,
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

enum class ZclStatus : uint8_t {
	SUCCESS = 0x00,
	UNSUPPORTED_ATTRIBUTE = 0x86
};

enum class ZclDataType : uint8_t  {
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
enum class ZclBasicAttribute : uint16_t {
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
enum class ZclBasicPowerSourceType : uint8_t {
	// mains (single phase)
	MAINS = 1,

	// battery
	BATTERY = 3
};

// power configuration cluster
// ---------------------------

// attributes of power configuration cluster
enum class ZclPowerConfigurationAttribute : uint16_t {
	BATTERY_VOLTAGE = 0x0020, // (INT8U)
	BATTERY_PERCENTAGE = 0x0021, // (INT8U)
	REVISION = 0xFFFD, // cluster revision (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

// on off cluster
// --------------

// attributes of on off cluster
enum class ZclOnOffAttribute : uint16_t {
	ON_OFF = 0x0000, // (BOOL, required)
	GLOBAL_SCENE_CONTROL = 0x4000, // (BOOL)
	ON_TIME = 0x4001, // INT16U
	OFF_WAIT_TIME = 0x4002, // INT16U
	CLUSTER_REVISION = 0xFFFD, // (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

enum class ZclOnOffCommand : uint8_t {
	OFF = 0x00, // (required)
	ON = 0x01, // (required)
	TOGGLE = 0x02, // (required)
	OFF_WITH_EFFECT = 0x40,
	ON_WITH_RECALL_GLOBAL_SCENE = 0x41,
	ON_WITH_TIMED_OFF
};

// level control cluster
// ---------------------

enum class ZclLevelControlAttribute : uint16_t {
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

enum class ZclLevelControlCommand : uint8_t {
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

enum class ZclColorControlAttribute : uint16_t {
	COLOR_X = 0x0003, // current color, x component of CIE xyY color model (INT16U / 65536, read only, required)
	COLOR_Y = 0x0004, // current color, y component of CIE xyY color model (INT16U / 65536, read only, required)
	OPTIONS = 0x000F, // options (BITMAP8, required)
	COUPLE_COLOR_TEMP_TO_LEVEL_MIN_MIREDS = 0x400D, // (INT16U, required)
	START_UP_COLOR_TEMP_MIREDS = 0x4010,
	CLUSTER_REVISION = 0xFFFD, // (INT16U, required)
	REPORTING_STATUS = 0xFFFE // (ENUM8)
};

enum class ZclColorControlCommand : uint16_t {
	MOVE_TO_COLOR = 0x07, // (required)
	MOVE_COLOR = 0x08, // (required)
	STEP_COLOR = 0x09, // (required)
	STOP_MOVE_STEP = 0x47
};

// thermostat
// ----------

enum class ZclThermostatAttribute : uint16_t {
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

enum class ZclThermostatCommand : uint16_t {

};


// security
// --------

// standard key type (table 4.9)
enum class StandardKeyType : uint8_t  {
	// network key used in nwk, is defined by the coordinator and distributed with TRANSPORT_KEY aps messages
	NETWORK = 1,

	// application link key used in aps for communication between two devices
	APPLICATION_LINK = 3,

	// trust-center link key used in aps, default is 5A:69:67:42:65:65:41:6C:6C:69:61:6E:63:65:30:39
	TRUST_CENTER_LINK = 4
};

// request key type (table 4.19)
enum class RequestKeyType : uint8_t {
	APPLICATION_LINK = 2,
	TRUST_CENTER_LINK = 4
};

// security control field for nwk and aps (4.5.1.1)
enum class SecurityControl : uint8_t {
	// security level (table 4.30)
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
	KEY_LINK = 0, // trust-center or application link key used in aps
	KEY_NETWORK = 1 << 3, // network key used in nwk
	KEY_KEY_TRANSPORT = 2 << 3, // used to aps-encrypt TRANSPORT_KEY messages containing a network key
	KEY_KEY_LOAD = 3 << 3, // used to aps-encrypt TRANSPORT_KEY messages containing a link key

	EXTENDED_NONCE = 1 << 5,
};
FLAGS_ENUM(SecurityControl)

} // namespace zb
