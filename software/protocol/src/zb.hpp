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
