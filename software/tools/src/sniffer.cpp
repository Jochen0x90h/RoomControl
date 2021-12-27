#include <sys.hpp>
#include <radioDefs.hpp>
#include <usbDefs.hpp>
#include <crypt.hpp>
#include <hash.hpp>
#include <Nonce.hpp>
#include <ieee.hpp>
#include <zb.hpp>
#include <gp.hpp>
#include <enum.hpp>
#include <DataReader.hpp>
#include <StringBuffer.hpp>
#include <util.hpp>
#include <boost/filesystem.hpp>
#include <array>
#include <map>
#include <string>
#include <chrono>
#include <libusb.h>


// logs ieee 802.15.4 traffic to a .pcap file

// PTM 215Z/216Z: Press A0 for 7 seconds to commission on channel 15, then A1 and A0 together to confirm

namespace fs = boost::filesystem;

// header of pcap file
struct PcapHeader {
	uint32_t magic_number;   // magic number
	uint16_t version_major;  // major version number
	uint16_t version_minor;  // minor version number
	int32_t thiszone;       // GMT to local correction
	uint32_t sigfigs;        // accuracy of timestamps
	uint32_t snaplen;        // max length of captured packets, in octets
	uint32_t network;        // data link type
};

// header of one packet in a pcap file
struct PcapPacket {
	uint32_t ts_sec;         // timestamp seconds
	uint32_t ts_usec;        // timestamp microseconds
	uint32_t incl_len;       // number of octets of packet saved in file
	uint32_t orig_len;       // actual length of packet
	uint8_t data[140];
};

// zbee alliance 2009 key
static uint8_t const za09Key[] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};

// zbee light link key
static uint8_t const zllKey[] = {0x81, 0x42, 0x86, 0x86, 0x5D, 0xC1, 0xC8, 0xB2, 0xC8, 0xCB, 0xC5, 0x2E, 0x5D, 0x65, 0xD1, 0xB8};

// bus default key
static uint8_t const busKey[] = {0x13, 0x37, 0xc6, 0xb3, 0xf1, 0x6c, 0x7c, 0xb6, 0x2d, 0xec, 0x18, 0x2d, 0x30, 0x78, 0xd6, 0x18};

AesKey trustCenterLinkAesKey;
AesKey hashedTrustCenterLinkAesKey;
AesKey networkAesKey;
constexpr zb::SecurityControl securityLevel = zb::SecurityControl::LEVEL_ENC_MIC32; // encrypted + 32 bit message integrity code



// print tinycrypt aes key
void printKey(char const *name, AesKey const &key) {
	sys::out.write("AesKey const " + str(name) + " = {{");
	bool first = true;
	for (auto w : key.words) {
		if (!first)
			sys::out.write(", ");
		first = false;
		sys::out.write("0x" + hex(w));
	}
	sys::out.write("}};\n");
}

using PacketReader = DecryptReader;

void handleGp(uint8_t const *mac, PacketReader &r);
void handleGpCommission(uint32_t deviceId, PacketReader &r);

void handleNwk(PacketReader &r);
void handleAps(PacketReader &r, uint8_t const *extendedSource);
void handleZdp(PacketReader &r);
void handleZcl(PacketReader &r, uint8_t destinationEndpoint);


void handleIeee(PacketReader &r) {
	// ieee 802.15.4 mac
	// -----------------
	uint8_t const *mac = r.current;

	// frame control
	auto frameControl = r.enum16<ieee::FrameControl>();

	uint8_t sequenceNumber = 0;
	if ((frameControl & ieee::FrameControl::SEQUENCE_NUMBER_SUPPRESSION) == 0) {
		sequenceNumber = r.uint8();
		sys::out.write("Seq " + dec(sequenceNumber) + "; ");
	}

	// destination pan/address
	bool haveDestination = (frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_FLAG) != 0;
	if (haveDestination) {
		// destination address present
		
		// destination pan
		uint16_t destinationPan = r.uint16();
		sys::out.write(hex(destinationPan) + ':');
	
		// check if short or long addrssing
		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
			// short destination address
			uint16_t destination = r.uint16();
			
			sys::out.write(hex(destination));
		} else {
			// long destination address
			uint64_t destination = r.int64();
			
			sys::out.write(hex(destination));
		}
		sys::out.write(" <- ");
	}
	
	// source pan/address
	bool haveSource = (frameControl & ieee::FrameControl::SOURCE_ADDRESSING_FLAG) != 0;
	if (haveSource) {
		// source address present
		
		// check if pan is present
		if ((frameControl & ieee::FrameControl::PAN_ID_COMPRESSION) == 0 || !haveDestination) {
			uint16_t sourcePan = r.uint16();
			sys::out.write(hex(sourcePan) + ':');
		}
	
		// check if short or long addrssing
		if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0) {
			// short source address
			uint16_t source = r.uint16();
			
			sys::out.write(hex(source));
		} else {
			// long source address
			uint64_t source = r.int64();
			
			sys::out.write(hex(source));
		}
	}
	if (haveDestination || haveSource)
		sys::out.write("; ");

	auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;
	switch (frameType) {
	case ieee::FrameControl::TYPE_BEACON:
		{
			uint16_t superframeSpecification = r.uint16();
			
			uint8_t gts = r.uint8();
			
			uint8_t pending = r.uint8();
			
			uint8_t protocolId = r.uint8();
			
			uint16_t stackProfile = r.uint16();
			int type = stackProfile & 15;
			int protocolVersion = (stackProfile >> 4) & 15;
			bool routerFlag = stackProfile & 0x400;
			
			sys::out.write("Beacon: " + dec(type) + ", " + dec(protocolVersion));
			if (routerFlag)
				sys::out.write(", router");
			sys::out.write("\n");
		}
		break;
	case ieee::FrameControl::TYPE_COMMAND:
		{
			auto command = r.enum8<ieee::Command>();
			
			switch (command) {
			case ieee::Command::ASSOCIATION_REQUEST:
				sys::out.write("Association Request\n");
				break;
			case ieee::Command::ASSOCIATION_RESPONSE:
				sys::out.write("Association Response\n");
				break;
			case ieee::Command::DATA_REQUEST:
				sys::out.write("Data Request\n");
				break;
			case ieee::Command::ORPHAN_NOTIFICATION:
				sys::out.write("Orphan Notification\n");
				break;
			case ieee::Command::BEACON_REQUEST:
				sys::out.write("Beacon Request\n");
				break;
			default:
				sys::out.write("Unknown Command\n");
			}
		}
		break;
	case ieee::FrameControl::TYPE_ACK:
		sys::out.write("Ack\n");
		break;
	case ieee::FrameControl::TYPE_DATA:
		{
			auto version = r.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
			switch (version) {
			case gp::NwkFrameControl::VERSION_2:
				handleNwk(r);
				break;
			case gp::NwkFrameControl::VERSION_3_GP:
				handleGp(mac, r);
				break;
			default:
				sys::out.write("Unknown Nwk Frame Version!\n");
			}
		}
		break;
	default:
		sys::out.write("Unknown IEEE Frame Type\n");
	}
}


// Self-powered Devices
// --------------------

struct GpDevice {
	AesKey aesKey;
};

std::map<uint32_t, GpDevice> gpDevices;

void handleGp(uint8_t const *mac, PacketReader &r) {
	// zgp stub nwk header
	
	// set start of header
	r.setHeader();

	// frame control
	auto frameControl = r.enum8<gp::NwkFrameControl>();

	// extended frame conrol
	gp::NwkExtendedFrameControl extendedFrameControl = gp::NwkExtendedFrameControl::NONE;
	if ((frameControl & gp::NwkFrameControl::EXTENDED) != 0)
		extendedFrameControl = r.enum8<gp::NwkExtendedFrameControl>();
	auto securityLevel = extendedFrameControl & gp::NwkExtendedFrameControl::SECURITY_LEVEL_MASK;

	// device id
	uint32_t deviceId = r.int32();
	sys::out.write("Device Id: " + hex(deviceId) + "; ");

	if (securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_NONE
		&& r.peekEnum8<gp::Command>() == gp::Command::COMMISSIONING)
	{
		// commissioning (PTM215Z/216Z: hold A0 down for 7 seconds)
		handleGpCommission(deviceId, r);
		return;
	}

	// get device (when the device was not commissioned, decryption will fail)
	GpDevice &device = gpDevices[deviceId];

	// security
	// --------
	// header: header that is not encrypted, payload is part of header for security levels 0 and 1
	// payload: payload that is encrypted, has zero length for security levels 0 and 1
	// mic: message integrity code, 2 or 4 bytes
	uint32_t securityCounter;
	int micLength = 4;
	switch (securityLevel) {
	case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT8_MIC16:
		// security level 1: 1 byte counter, 2 byte mic
	
		// header starts at mac sequence number and includes also payload
		r.setHeader(mac + 2);
		
		// use mac sequence number as security counter
		securityCounter = mac[2];
		
		micLength = 2;
		
		// encrypted message is empty, only message integrity code
		r.setMessage(r.end - micLength);
		break;
	case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32:
		// security level 2: 4 byte counter, 4 byte mic

		// security counter
		securityCounter = r.int32();

		// encrypted message is empty, only message integrity code
		r.setMessage(r.end - micLength);
		break;
	case gp::NwkExtendedFrameControl::SECURITY_LEVEL_ENC_CNT32_MIC32:
		// security level 3: 4 byte counter, encrypted message, 4 byte mic

		// security counter
		securityCounter = r.int32();

		// set start of encrypted message
		r.setMessage();
		break;
	default:
		// security is required
		return;
	}
	
	// check message integrity code or decrypt message, depending on security level
	Nonce nonce(deviceId, securityCounter);
	if (!r.decrypt(micLength, nonce, device.aesKey)) {
		if (securityLevel <= gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32) {
			sys::out.write("Decrypt Error; ");
			// we can continue as data is not encrypted
		} else {
			sys::out.write("Error while decrypting message!\n");
			return;
		}
	}

	sys::out.write("Data:");
	int l = r.getRemaining();//end - d;
	for (int i = 0; i < l; ++i)
		sys::out.write(' ' + hex(r.current[i]));
	sys::out.write("\n");
}

void handleGpCommission(uint32_t deviceId, PacketReader &r) {
	// remove commissioning command (0xe0)
	r.uint8();

	GpDevice device;

	// A.4.2.1.1 Commissioning
		
	// device type
	// 0x02: on/off switch
	auto deviceType = r.enum8<gp::DeviceType>();

	// options
	auto options = r.enum8<gp::Options>();
	if ((options & gp::Options::EXTENDED) != 0) {
		auto extendedOptions = r.enum8<gp::ExtendedOptions>();;
		
		// security level capability (used in messages)
		auto securityLevel = extendedOptions & gp::ExtendedOptions::SECURITY_LEVEL_CAPABILITY_MASK;

		// check for key
		if ((extendedOptions & gp::ExtendedOptions::KEY_PRESENT) != 0) {
			uint8_t *key = r.current;
			if ((extendedOptions & gp::ExtendedOptions::KEY_ENCRYPTED) != 0) {
				// Green Power A.1.5.3.3.3
				
				// nonce
				Nonce nonce(deviceId, deviceId);

				// construct a header containing the device id
				DataBuffer<4> header;
				header.setLittleEndianInt32(0, deviceId);
				
				if (!decrypt(key, nonce, header.data, 4, key, 16, 4, trustCenterLinkAesKey)) {
					sys::out.write("Error while decrypting key!\n");
					return;
				}
				
				// skip key and MIC
				r.skip(16 + 4);
			} else {
				// skip key
				r.skip(16);
			}

			// print key
			sys::out.write("Key: ");
			for (int i = 0; i < 16; ++i) {
				if (i > 0)
					sys::out.write(":");
				sys::out.write(hex(key[i]));
			}
			sys::out.write("\n");

			// set key for device
			setKey(device.aesKey, key);
		}
		if ((extendedOptions & gp::ExtendedOptions::COUNTER_PRESENT) != 0) {
			uint32_t counter = r.int32();
			sys::out.write("Counter: 0x" + hex(counter) + "\n");
		}
	}

	// check if we exceeded the end
	if (r.getRemaining() < 0)
		return;

	switch (deviceType) {
	case gp::DeviceType::ON_OFF_SWITCH:
		// hue switch
		
		break;
	case gp::DeviceType::GENERIC_SWITCH:
		// generic switch
		
		break;
	}
	
	gpDevices[deviceId] = device;
}


// zbee
// ----

void handleNwk(PacketReader &r) {
	// nwk header

	// set start of header
	r.setHeader();

	auto frameControl = r.enum16<zb::NwkFrameControl>();
	uint16_t destination = r.uint16();
	uint16_t source = r.uint16();
	uint8_t radius = r.uint8();
	uint8_t nwkCounter = r.uint8();
	sys::out.write(hex(destination) + " <- " + hex(source) + " (r " + dec(radius) + "); ");

	// destination
	if ((frameControl & zb::NwkFrameControl::DESTINATION) != 0) {
		uint8_t const *destination = r.current;
		r.skip(8);
	}

	// extended source
	if ((frameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0) {
		uint8_t const *extendedSource = r.current;
		r.skip(8);
	}
	
	// source route
	if ((frameControl & zb::NwkFrameControl::SOURCE_ROUTE) != 0) {
		uint8_t relayCount = r.uint8();
		uint8_t relayIndex = r.uint8();
		sys::out.write("source route");
		for (int i = 0; i < relayCount; ++i) {
			uint16_t relay = r.uint16();
			sys::out.write(' ' + hex(relay));
		}
		sys::out.write("; ");
	}
	
	// security header
	// note: does in-place decryption of the payload
	uint8_t const *extendedSource = nullptr;
	if ((frameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// restore security level according to 4.3.1.2 step 1.
		r.current[0] |= securityLevel;
		
		// security control field (4.5.1.1)
		auto securityControl = r.enum8<zb::SecurityControl>();
				
		// security counter
		uint32_t securityCounter = r.int32();
		sys::out.write("SecCnt " + hex(securityCounter) + "; ");

		// key type
		if ((securityControl & zb::SecurityControl::KEY_MASK) != zb::SecurityControl::KEY_NETWORK) {
			sys::out.write("Error: Only network key supported!\n");
			return;
		}
		
		// extended source
		if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
			sys::out.write("Error: Only extended nonce supported!\n");
			return;
		}
		extendedSource = r.current;
		r.skip(8);
		
		// key sequence number
		uint8_t keySequenceNumber = r.uint8();
		
		// set start of message
		r.setMessage();

		// nonce (4.5.2.2)
		Nonce nonce(extendedSource, securityCounter, securityControl);

		// decrypt
		int micLength = 4;
		if (!r.decrypt(micLength, nonce, networkAesKey)) {
			sys::out.write("Error: Decryption failed!\n");
			return;
		}
	}
	
	auto frameType = frameControl & zb::NwkFrameControl::TYPE_MASK;
	if (frameType == zb::NwkFrameControl::TYPE_COMMAND) {
		// nwk command
		auto command = r.enum8<zb::NwkCommand>();
		switch (command) {
		case zb::NwkCommand::ROUTE_REQUEST:
			sys::out.write("Route Request ");
			{
				auto options = r.enum8<zb::NwkCommandRouteRequestOptions>();
				uint8_t id = r.uint8();
				uint16_t destination = r.uint16();
				uint8_t cost = r.uint8();
				
				switch (options & zb::NwkCommandRouteRequestOptions::DISCOVERY_MASK) {
				case zb::NwkCommandRouteRequestOptions::DISCOVERY_SINGLE:
					sys::out.write(hex(destination));
					if ((options & zb::NwkCommandRouteRequestOptions::EXTENDED_DESTINATION) != 0)
						sys::out.write(" ext");
					break;
				case zb::NwkCommandRouteRequestOptions::DISCOVERY_MANY_TO_ONE_WITH_SOURCE_ROUTING:
					// https://www.digi.com/resources/documentation/Digidocs/90002002/Concepts/c_zb_many_to_one_routing.htm
					sys::out.write("Many-to-One");
					break;
				default:
					break;
				}
			}
			sys::out.write("\n");
			break;
		case zb::NwkCommand::ROUTE_REPLY:
			sys::out.write("Route Reply ");
			{
				auto options = r.enum8<zb::NwkCommandRouteReplyOptions>();
				uint8_t id = r.uint8();
				uint16_t originator = r.uint16();
				uint16_t responder = r.uint16();
				uint8_t cost = r.uint8();
			}
			sys::out.write("\n");
			break;
		case zb::NwkCommand::LEAVE:
			sys::out.write("Leave\n");
			break;
		case zb::NwkCommand::ROUTE_RECORD:
			// https://www.digi.com/resources/documentation/Digidocs/90001942-13/concepts/c_source_routing.htm
			sys::out.write("Route Record:");
			{
				uint8_t relayCount = r.uint8();
				for (int i = 0; i < relayCount; ++i) {
					uint16_t relay = r.uint16();
					sys::out.write(' ' + hex(relay));
				}
			}
			sys::out.write("\n");
			break;
		case zb::NwkCommand::REJOIN_REQUEST:
			sys::out.write("Rejoin Request\n");
			break;
		case zb::NwkCommand::REJOIN_RESPONSE:
			sys::out.write("Rejoin Response\n");
			break;
		case zb::NwkCommand::LINK_STATUS:
			sys::out.write("Link Status\n");
			break;
		default:
			sys::out.write("Unknown NWK Command\n");
		}
	} else if (frameType == zb::NwkFrameControl::TYPE_DATA) {
		// nwk data
		handleAps(r, extendedSource);
	} else {
		sys::out.write("Unknown NWK Frame Type!\n");
	}
}

void handleAps(PacketReader &r, uint8_t const *extendedSource) {
	// application support layer
	
	// set start of header
	r.setHeader();

	// frame control
	auto frameControl = r.enum8<zb::ApsFrameControl>();
	auto frameType = frameControl & zb::ApsFrameControl::TYPE_MASK;
	if (frameType == zb::ApsFrameControl::TYPE_COMMAND) {
		// aps command
		uint8_t apsCounter = r.uint8();
		
		// security header
		// note: does in-place decryption of the payload
		if ((frameControl & zb::ApsFrameControl::SECURITY) != 0) {
			// restore security level according to 4.4.1.2 step 5.
			r.current[0] |= securityLevel;
			
			// security control field (4.5.1.1)
			auto securityControl = r.enum8<zb::SecurityControl>();
			
			// security counter
			uint32_t securityCounter = r.int32();

			auto keyType = securityControl & zb::SecurityControl::KEY_MASK;
			if (keyType != zb::SecurityControl::KEY_LINK && keyType != zb::SecurityControl::KEY_KEY_TRANSPORT) {
				sys::out.write("Error: Only link key and key transport key supported!\n");
				return;
			}
			if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
				if (extendedSource == nullptr) {
					sys::out.write("Error: Only extended nonce supported!\n");
					return;
				}
			} else {
				extendedSource = r.current;
				r.skip(8);
			}

			// set start of message
			r.setMessage();

			// nonce (4.5.2.2)
			Nonce nonce(extendedSource, securityCounter, securityControl);

			// decrypt
			int micLength = 4;
			if (!r.decrypt(micLength, nonce, keyType == zb::SecurityControl::KEY_LINK ? trustCenterLinkAesKey : hashedTrustCenterLinkAesKey)) {
				sys::out.write("Error: Decryption failed!\n");
				return;
			}
		}
		
		auto command = r.enum8<zb::ApsCommand>();
		switch (command) {
		case zb::ApsCommand::TRANSPORT_KEY:
			{
				sys::out.write("Transport Key\n");
				auto newKeyIdentifier = r.enum8<zb::KeyIdentifier>();

				uint8_t const *newKey = r.current;
				r.skip(16);
				
				uint8_t newKeySequenceNumber = r.uint8();
				
				uint8_t const *extendedDestination = r.current;
				r.skip(8);
				
				uint8_t const *extendedSource = r.current;
				r.skip(8);
				
				if (newKeyIdentifier == zb::KeyIdentifier::NETWORK) {
					setKey(networkAesKey, newKey);
				}
			}
			break;
		case zb::ApsCommand::UPDATE_DEVICE:
			{
				sys::out.write("Update Device\n");
			}
			break;
		default:
			sys::out.write("Unknown APS Command!\n");
		}
	} else if (frameType == zb::ApsFrameControl::TYPE_DATA) {
		// aps data: zdp or zcl follow
		uint8_t destinationEndpoint = r.uint8();
		if (destinationEndpoint == 0)
			handleZdp(r);
		else
			handleZcl(r, destinationEndpoint);
	} else if (frameType == zb::ApsFrameControl::TYPE_ACK) {
		// aps ack
		sys::out.write("Ack\n");
	} else {
		sys::out.write("Unknown APS Frame Type!\n");
	}
}

void handleZdp(PacketReader &r) {
	zb::ZdpCommand command = r.enum16<zb::ZdpCommand>();
	uint16_t profile = r.uint16();
	uint8_t sourceEndpoint = r.uint8();
	uint8_t apsCounter = r.uint8();

	switch (command) {
	case zb::ZdpCommand::NETWORK_ADDRESS_REQUEST:
		sys::out.write("Network Address Request\n");
		break;
	case zb::ZdpCommand::EXTENDED_ADDRESS_REQUEST:
		sys::out.write("Extended Address Request\n");
		break;
	case zb::ZdpCommand::EXTENDED_ADDRESS_RESPONSE:
		sys::out.write("Extended Address Response\n");
		break;
	case zb::ZdpCommand::NODE_DESCRIPTOR_REQUEST:
		sys::out.write("Node Descriptor Request\n");
		break;
	case zb::ZdpCommand::NODE_DESCRIPTOR_RESPONSE:
		sys::out.write("Node Descriptor Response\n");
		break;
	case zb::ZdpCommand::SIMPLE_DESCRIPTOR_REQUEST:
		sys::out.write("Simple Descriptor Request\n");
		break;
	case zb::ZdpCommand::SIMPLE_DESCRIPTOR_RESPONSE:
		sys::out.write("Simple Descriptor Response\n");
		break;
	case zb::ZdpCommand::ACTIVE_ENDPOINT_REQUEST:
		sys::out.write("Active Endpoint Request\n");
		break;
	case zb::ZdpCommand::ACTIVE_ENDPOINT_RESPONSE:
		sys::out.write("Active Endpoint Response\n");
		break;
	case zb::ZdpCommand::MATCH_DESCRIPTOR_REQUEST:
		sys::out.write("Match Descriptor Request\n");
		break;
	case zb::ZdpCommand::MATCH_DESCRIPTOR_RESPONSE:
		sys::out.write("Match Descriptor Response\n");
		break;
	case zb::ZdpCommand::DEVICE_ANNOUNCEMENT:
		sys::out.write("Device Announcement\n");
		break;
	case zb::ZdpCommand::BIND_REQUEST:
		sys::out.write("Bind Request\n");
		break;
	case zb::ZdpCommand::BIND_RESPONSE:
		sys::out.write("Bind Response\n");
		break;
	case zb::ZdpCommand::PERMIT_JOIN_REQUEST:
		sys::out.write("Permit Joint Request\n");
		break;
	default:
		sys::out.write("Unknown ZDP Command 0x" + hex(command) + "\n");
	}
}

void handleZcl(PacketReader &r, uint8_t destinationEndpoint) {
	zb::ZclCluster cluster = r.enum16<zb::ZclCluster>();
	zb::ZclProfile profile = r.enum16<zb::ZclProfile>();
	uint8_t sourceEndpoint = r.uint8();
	uint8_t apsCounter = r.uint8();

	// cluster library frame
	auto frameControl = r.enum8<zb::ZclFrameControl>();
	auto frameType = frameControl & zb::ZclFrameControl::TYPE_MASK;
	bool manufacturerSpecificFlag = (frameControl & zb::ZclFrameControl::MANUFACTURER_SPECIFIC) != 0;
	//bool directionFlag = frameControl & 0x80; // false: client to server, true: server to client

	uint8_t zclCounter = r.uint8();
	
	sys::out.write("ZclCnt " + dec(zclCounter) + "; ");

	if (frameType == zb::ZclFrameControl::TYPE_PROFILE_WIDE && !manufacturerSpecificFlag) {
		auto command = r.enum8<zb::ZclCommand>();
		switch (command) {
		case zb::ZclCommand::CONFIGURE_REPORTING:
			sys::out.write("Configure Reporting\n");
			break;
		case zb::ZclCommand::CONFIGURE_REPORTING_RESPONSE:
			sys::out.write("Configure Reporting Response\n");
			break;
		case zb::ZclCommand::READ_ATTRIBUTES:
			sys::out.write("Read Attributes; ");
			switch (cluster) {
			case zb::ZclCluster::BASIC:
				{
					auto attribute = r.enum16<zb::ZclBasicAttribute>();

					switch (attribute) {
					case zb::ZclBasicAttribute::MODEL_IDENTIFIER:
						sys::out.write("Model Identifier\n");
						break;
					default:
						sys::out.write("Unknown Attribute\n");
					}
				}
				break;
			case zb::ZclCluster::POWER_CONFIGURATION:
				{
					auto attribute = r.enum16<zb::ZclPowerConfigurationAttribute>();

					switch (attribute) {
					case zb::ZclPowerConfigurationAttribute::BATTERY_VOLTAGE:
						sys::out.write("Battery Voltage\n");
						break;
					default:
						sys::out.write("Unknown Attribute\n");
					}
				}
				break;
			case zb::ZclCluster::IDENTIFY:
				sys::out.write("Unknown Attribute\n");
				break;
			case zb::ZclCluster::ON_OFF:
				sys::out.write("Unknown Attribute\n");
				break;
			default:
				sys::out.write("Unknown Attribute\n");
			}
			break;
		case zb::ZclCommand::READ_ATTRIBUTES_RESPONSE:
			sys::out.write("Read Attributes Response; ");
			switch (cluster) {
			case zb::ZclCluster::BASIC:
				{
					auto attribute = r.enum16<zb::ZclBasicAttribute>();
					uint8_t status = r.uint8();
					
					if (status == 0) {
						auto dataType = r.enum8<zb::ZclDataType>();
														
						switch (attribute) {
						case zb::ZclBasicAttribute::MODEL_IDENTIFIER:
							{
								sys::out.write("Model Identifier: " + String(r.getRemaining(), r.current) + '\n');
							}
							break;
						default:
							sys::out.write("Unknown\n");
						}
					} else {
						sys::out.write("Failed\n");
					}
				}
				break;
			case zb::ZclCluster::POWER_CONFIGURATION:
				{
					auto attribute = r.enum16<zb::ZclPowerConfigurationAttribute>();
					uint8_t status = r.uint8();
					
					if (status == 0) {
						auto dataType = r.enum8<zb::ZclDataType>();
														
						switch (attribute) {
						case zb::ZclPowerConfigurationAttribute::BATTERY_VOLTAGE:
							{
								int value;
								switch (dataType) {
								case zb::ZclDataType::UINT8:
									value = uint8_t(r.uint8());
									break;
								default:
									value = 0;
								}

								sys::out.write("Battery Voltage " + dec(value / 10) + '.' + dec(value % 10) + "V\n");
							}
							break;
						default:
							sys::out.write("Unknown\n");
						}
					} else {
						sys::out.write("Failed\n");
					}
				}
				break;
			case zb::ZclCluster::IDENTIFY:
				sys::out.write("Unknown Attribute\n");
				break;
			case zb::ZclCluster::ON_OFF:
				sys::out.write("Unknown Attribute\n");
				break;
			default:
				sys::out.write("Unknown Attribute\n");
			}
			break;
		case zb::ZclCommand::DEFAULT_RESPONSE:
			sys::out.write("Default Response\n");
			break;
		default:
			sys::out.write("Unknown ZCL Command\n");
		}
	} else if (frameType == zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC && !manufacturerSpecificFlag) {
		switch (cluster) {
		case zb::ZclCluster::BASIC:
			sys::out.write("Cluster: Basic\n");
			break;
		case zb::ZclCluster::POWER_CONFIGURATION:
			sys::out.write("Cluster: Power Configuration\n");
			break;
		case zb::ZclCluster::ON_OFF:
			sys::out.write("Cluster: On/Off; ");
			{
				uint8_t command = r.uint8();
				switch (command) {
				case 0:
					sys::out.write("Off\n");
					break;
				case 1:
					sys::out.write("On\n");
					break;
				case 2:
					sys::out.write("Toggle\n");
					break;
				default:
					sys::out.write("Unknown Command\n");
				}
			}
			break;
		case zb::ZclCluster::GREEN_POWER:
			sys::out.write("Cluster: Green Power; ");
			{
				uint8_t command = r.uint8();
				switch (command) {
				case 2:
					sys::out.write("GP Proxy Commissioning Mode\n");
					break;
				default:
					sys::out.write("Unknown Command\n");
				}
			}
			break;
		default:
			sys::out.write("Unknown Cluster 0x" + hex(cluster) + "\n");
		}
	} else {
		sys::out.write("Unknown ZCL Frame Type\n");
	}
}



int controlTransfer(libusb_device_handle *handle, radio::Request request, uint16_t wValue, uint16_t wIndex) {
	return libusb_control_transfer(handle,
		usb::OUT | usb::REQUEST_TYPE_VENDOR | usb::RECIPIENT_INTERFACE,
		uint8_t(request), wValue, wIndex,
		nullptr, 0, 1000);
}

int main(int argc, char const *argv[]) {
	fs::path inputFile;
	fs::path outputFile;
	
	//setKey(trustCenterLinkAesKey, busKey);
	//printKey("busDefaultAesKey", trustCenterLinkAesKey);

	setKey(trustCenterLinkAesKey, za09Key);
	//printKey("trustCenterLinkAesKey", trustCenterLinkAesKey);

	DataBuffer<16> hashedKey;
	keyHash(hashedKey, za09Key, 0);
	setKey(hashedTrustCenterLinkAesKey, hashedKey);
	//printKey("hashedTrustCenterLinkAesKey", hashedTrustCenterLinkAesKey);
	
	int radioChannel = 15;
	auto radioFlags = radio::ContextFlags::PASS_ALL;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		
		if (arg == "-k" || arg == "--key") {
			// get network key from argument
			++i;
			int k[16];
			sscanf(argv[i], "%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
				&k[0], &k[1], &k[2], &k[3], &k[4], &k[5], &k[6], &k[7],
				&k[8], &k[9], &k[10], &k[11], &k[12], &k[13], &k[14], &k[15]);
			
			uint8_t key[16];
			for (int i = 0; i < 16; ++i)
				key[i] = k[i];
			
			setKey(networkAesKey, key);
		} else if (arg == "-i" || arg == "--input") {
			// input pcap file
			++i;
			inputFile = argv[i];
		} else if (arg == "-c" || arg == "--channel") {
			// radio channel
			++i;
			radioChannel = atoi(argv[i]);
		} else if (arg == "-a" || arg == "--ack") {
			// ack all received packets
			radioFlags |= radio::ContextFlags::HANDLE_ACK;
		} else {
			// output pcap file
			outputFile = argv[i];
		}
	}

	// either read from input .pcap file or from usb device
	if (inputFile.empty()) {
		// read from usb
		int r = libusb_init(NULL);
		if (r < 0)
			return r;

		// get device list
		libusb_device **devs;
		ssize_t cnt = libusb_get_device_list(NULL, &devs);
		if (cnt < 0) {
			libusb_exit(NULL);
			return (int)cnt;
		}

		// iterate over devices
		for (int i = 0; devs[i]; ++i) {
			libusb_device *dev = devs[i];

			// get device descriptor
			libusb_device_descriptor desc;
			int ret = libusb_get_device_descriptor(dev, &desc);
			if (ret != LIBUSB_SUCCESS) {
				sys::err.write("get device descriptor error: " + dec(ret) + '\n');
				continue;
			}
			
			// check vendor/product
			if (desc.idVendor != 0x1915 && desc.idProduct != 0x1337)
				continue;

			// protocol: 0: binary, 1: text
			bool text = desc.bDeviceProtocol == 1;

			// open device
			libusb_device_handle *handle;
			ret = libusb_open(dev, &handle);
			if (ret != LIBUSB_SUCCESS) {
				sys::err.write("open error: " + dec(ret) + '\n');
				continue;
			}
				
			// set configuration (reset alt_setting, reset toggles)
			ret = libusb_set_configuration(handle, 1);
			if (ret != LIBUSB_SUCCESS) {
				sys::err.write("set configuration error: " + dec(ret) + '\n');
				continue;
			}

			// claim interface with bInterfaceNumber = 0
			ret = libusb_claim_interface(handle, 0);
			if (ret != LIBUSB_SUCCESS) {
				sys::err.write("claim interface error: " + dec(ret) + '\n');
				continue;
			}

			//ret = libusb_set_interface_alt_setting(handle, 0, 0);

			PcapHeader header;
			PcapPacket packet;
			int length;

			// reset the radio
			controlTransfer(handle, radio::Request::RESET, 0, 0);
			while (libusb_bulk_transfer(handle, 1 | usb::IN, packet.data, sizeof(packet.data), &length, 10) == LIBUSB_SUCCESS)
				;
			
			// configure the radio
			controlTransfer(handle, radio::Request::START, radioChannel, 0);
			controlTransfer(handle, radio::Request::ENABLE_RECEIVER, 1, 0);
			controlTransfer(handle, radio::Request::SET_FLAGS, uint16_t(radioFlags), 0);

			// open output pcap file
			FILE *pcap = fopen(outputFile.string().c_str(), "wb");
			
			// write pcap header
			header.magic_number = 0xa1b2c3d4;
			header.version_major = 2;
			header.version_minor = 4;
			header.thiszone = 0;
			header.sigfigs = 0;
			header.snaplen = 128;
			header.network = 0xC3;
			fwrite(&header, sizeof(header), 1, pcap);

			// loop
			sys::out.write("waiting for ieee802.15.4 packet from device ...\n");
			//auto startTime = std::chrono::steady_clock::now();
			uint32_t startTime = -1;
			while (true) {
				std::fill(std::begin(packet.data), std::end(packet.data), 0xAA);
				
				// receive from radio
				ret = libusb_bulk_transfer(handle, 1 | usb::IN, packet.data, sizeof(packet.data), &length, 0);
				if (ret != LIBUSB_SUCCESS) {
					sys::err.write("transfer error: " + dec(ret) + '\n');
					break;
				}
				length -= 5;
				sys::out.write("length: " + dec(length) + ", LQI: " + dec(packet.data[length]) + '\n');
				uint8_t const *ts = &packet.data[length + 1];
				uint32_t timestamp = ts[0] | (ts[1] << 8) | (ts[2] << 16) | (ts[3] << 24);
				if (startTime == -1)
					startTime = timestamp;
				timestamp -= startTime;
				sys::out.write("timestamp: " + dec(timestamp / 1000000) + "." + dec(timestamp % 1000000) + '\n');

				if (length > 0) {
					// write to pcap file
					//auto now = std::chrono::steady_clock::now();
					//auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
					packet.ts_sec = timestamp / 1000000;
					packet.ts_usec = timestamp % 1000000;
					packet.incl_len = length;
					packet.orig_len = length + 2; // 2 byte crc not transferred
					fwrite(&packet, 1, offsetof(PcapPacket, data) + packet.incl_len, pcap);
					
					// flush file so that we can interrupt any time
					fflush(pcap);
				
					// dissect
					PacketReader r(packet.incl_len, packet.data);
					handleIeee(r);
				}
			}
			break;
		}
		libusb_free_device_list(devs, 1);
		libusb_exit(NULL);
	} else {
		// read from file
	
		// open input pcap file
		FILE *pcap = fopen(inputFile.string().c_str(), "rb");
		
		// read pcap header
		PcapHeader header;
		fread(&header, sizeof(header), 1, pcap);
		if (header.network != 0xC3) {
			sys::err.write("error: protocol not supported!\n");
		}

		// read packets
		PcapPacket packet;
		while (fread(&packet, offsetof(PcapPacket, data), 1, pcap) == 1) {
			// read packet
			fread(packet.data, 1, packet.incl_len, pcap);
			
			// dissect packet
			PacketReader r(packet.incl_len, packet.data);
			handleIeee(r);
		}
	}
	return 0;
}

