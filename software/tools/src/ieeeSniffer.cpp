#include <Terminal.hpp>
#include <RadioDefs.hpp>
#include <usb.hpp>
#include <crypt.hpp>
#include <hash.hpp>
#include <Nonce.hpp>
#include <ieee.hpp>
#include <zb.hpp>
#include <zcl.hpp>
#include <gp.hpp>
#include <pcap.hpp>
#include <MessageReader.hpp>
#include <StringBuffer.hpp>
#include <StringOperators.hpp>
#include <util.hpp>
#include <map>
#include <string>
#include <filesystem>
#include <libusb.h>


// logs ieee 802.15.4 traffic to a .pcap file

// PTM 215Z/216Z: Press A0 for 7 seconds to commission on channel 15, then A1 and A0 together to confirm

namespace fs = std::filesystem;

using Packet = pcap::Packet<140>;

// security level to use, encrypted + 32 bit message integrity code
constexpr zb::SecurityControl securityLevel = zb::SecurityControl::LEVEL_ENC_MIC32;


// network key used by nwk layer, prepared for aes encryption/decryption
AesKey networkAesKey;

// link key used by aps layer, prepared for aes encryption/decryption
// todo: one link key per device
AesKey linkAesKey;



// print tinycrypt aes key
void printKey(char const *name, AesKey const &key) {
	Terminal::out << ("AesKey const " + str(name) + " = {{");
	bool first = true;
	for (auto w : key.words) {
		if (!first)
			Terminal::out << ", ";
		first = false;
		Terminal::out << ("0x" + hex(w));
	}
	Terminal::out << "}};\n";
}



class PacketReader : public DecryptReader {
public:
	/**
	 * Construct on data
	 */
	PacketReader(int length, uint8_t *data) : DecryptReader(length, data) {}

	void restoreSecurityLevel(zb::SecurityControl securityLevel) {
		*this->current |= uint8_t(securityLevel);
	}
};



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
	auto frameControl = r.e16L<ieee::FrameControl>();

	uint8_t sequenceNumber = 0;
	if ((frameControl & ieee::FrameControl::SEQUENCE_NUMBER_SUPPRESSION) == 0) {
		sequenceNumber = r.u8();
		Terminal::out << ("Seq " + dec(sequenceNumber) + "; ");
	}

	// destination pan/address
	bool haveDestination = (frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_FLAG) != 0;
	if (haveDestination) {
		// destination address present

		// destination pan
		uint16_t destinationPan = r.u16L();
		Terminal::out << (hex(destinationPan) + ':');

		// check if short or long addrssing
		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
			// short destination address
			uint16_t destination = r.u16L();

			Terminal::out << (hex(destination));
		} else {
			// long destination address
			uint64_t destination = r.u64L();

			Terminal::out << (hex(destination));
		}
		Terminal::out << (" <- ");
	}

	// source pan/address
	bool haveSource = (frameControl & ieee::FrameControl::SOURCE_ADDRESSING_FLAG) != 0;
	if (haveSource) {
		// source address present

		// check if pan is present
		if ((frameControl & ieee::FrameControl::PAN_ID_COMPRESSION) == 0 || !haveDestination) {
			uint16_t sourcePan = r.u16L();
			Terminal::out << (hex(sourcePan) + ':');
		}

		// check if short or long addrssing
		if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0) {
			// short source address
			uint16_t source = r.u16L();

			Terminal::out << (hex(source));
		} else {
			// long source address
			uint64_t source = r.u64L();

			Terminal::out << (hex(source));
		}
	}
	if (haveDestination || haveSource)
		Terminal::out << ("; ");

	auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;
	switch (frameType) {
	case ieee::FrameControl::TYPE_BEACON:
		{
			uint16_t superframeSpecification = r.u16L();

			uint8_t gts = r.u8();

			uint8_t pending = r.u8();

			uint8_t protocolId = r.u8();

			uint16_t stackProfile = r.u16L();
			int type = stackProfile & 15;
			int protocolVersion = (stackProfile >> 4) & 15;
			bool routerFlag = stackProfile & 0x400;

			Terminal::out << ("Beacon: " + dec(type) + ", " + dec(protocolVersion));
			if (routerFlag)
				Terminal::out << (", router");
			Terminal::out << ("\n");
		}
		break;
	case ieee::FrameControl::TYPE_COMMAND:
		{
			auto command = r.e8<ieee::Command>();

			switch (command) {
			case ieee::Command::ASSOCIATION_REQUEST:
				Terminal::out << ("Association Request\n");
				break;
			case ieee::Command::ASSOCIATION_RESPONSE:
				Terminal::out << ("Association Response\n");
				break;
			case ieee::Command::DATA_REQUEST:
				Terminal::out << ("Data Request\n");
				break;
			case ieee::Command::ORPHAN_NOTIFICATION:
				Terminal::out << ("Orphan Notification\n");
				break;
			case ieee::Command::BEACON_REQUEST:
				Terminal::out << ("Beacon Request\n");
				break;
			default:
				Terminal::out << ("Unknown Command\n");
			}
		}
		break;
	case ieee::FrameControl::TYPE_ACK:
		Terminal::out << ("Ack\n");
		break;
	case ieee::FrameControl::TYPE_DATA:
		{
			auto version = r.peekE8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
			switch (version) {
			case gp::NwkFrameControl::VERSION_2:
				handleNwk(r);
				break;
			case gp::NwkFrameControl::VERSION_3_GP:
				handleGp(mac, r);
				break;
			default:
				Terminal::out << ("Unknown NWK Frame Version!\n");
			}
		}
		break;
	default:
		Terminal::out << ("Unknown IEEE Frame Type\n");
	}
}


// Self-powered Devices
// --------------------

struct GpDevice {
	AesKey aesKey;
};

std::map<uint32_t, GpDevice> gpDevices;

void handleGp(uint8_t const *mac, PacketReader &r) {
	// zgp stub nwk header (encryption header starts here)
	r.setHeader();

	// frame control
	auto frameControl = r.e8<gp::NwkFrameControl>();

	// extended frame conrol
	gp::NwkExtendedFrameControl extendedFrameControl = gp::NwkExtendedFrameControl::NONE;
	if ((frameControl & gp::NwkFrameControl::EXTENDED) != 0)
		extendedFrameControl = r.e8<gp::NwkExtendedFrameControl>();
	auto securityLevel = extendedFrameControl & gp::NwkExtendedFrameControl::SECURITY_LEVEL_MASK;

	// device id
	uint32_t deviceId = r.u32L();
	Terminal::out << "Device Id: " << hex(deviceId) << "; ";

	if (securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_NONE
		&& r.peekE8<gp::Command>() == gp::Command::COMMISSIONING)
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
	int micLength;
	switch (securityLevel) {
	case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT8_MIC16:
		// security level 1: 1 byte counter, 2 byte mic

		// header starts at mac sequence number and includes also payload
		r.setHeader(mac + 2);

		// use mac sequence number as security counter
		securityCounter = mac[2];

		// only decrypt message integrity code of length 2
		micLength = 2;
		r.setMessageFromEnd(micLength);
		break;
	case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32:
		// security level 2: 4 byte counter, 4 byte mic

		// security counter
		securityCounter = r.u32L();

		// only decrypt message integrity code of length 4
		micLength = 4;
		r.setMessageFromEnd(micLength);
		break;
	case gp::NwkExtendedFrameControl::SECURITY_LEVEL_ENC_CNT32_MIC32:
		// security level 3: 4 byte counter, encrypted message, 4 byte mic

		// security counter
		securityCounter = r.u32L();

		// decrypt message and message integrity code of length 4
		r.setMessage();
		micLength = 4;
		break;
	default:
		// security is required
		return;
	}

	// check message integrity code or decrypt message, depending on security level
	Nonce nonce(deviceId, securityCounter);
	if (!r.decrypt(micLength, nonce, device.aesKey)) {
		if (securityLevel <= gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32) {
			Terminal::out << ("Decrypt Error; ");
			// we can continue as data is not encrypted
		} else {
			Terminal::out << ("Error while decrypting message!\n");
			return;
		}
	}

	Terminal::out << ("Data:");
	int l = r.getRemaining();
	for (int i = 0; i < l; ++i)
		Terminal::out << (' ' + hex(r.current[i]));
	Terminal::out << ("\n");
}

void handleGpCommission(uint32_t deviceId, PacketReader &r) {
	// remove commissioning command (0xe0)
	r.u8();

	GpDevice device;

	// A.4.2.1.1 Commissioning

	// device type
	// 0x02: on/off switch
	auto deviceType = r.e8<gp::DeviceType>();

	// options
	auto options = r.e8<gp::Options>();
	if ((options & gp::Options::EXTENDED) != 0) {
		auto extendedOptions = r.e8<gp::ExtendedOptions>();;

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
				header.setU32L(0, deviceId);

				// in-place decrypt
				if (!decrypt(key, header.data, 4, key, 16, 4, nonce, zb::za09LinkAesKey)) {
					Terminal::out << ("Error while decrypting key!\n");
					return;
				}

				// skip key and MIC
				r.skip(16 + 4);
			} else {
				// skip key
				r.skip(16);
			}

			// print key
			Terminal::out << ("Key: ");
			for (int i = 0; i < 16; ++i) {
				if (i > 0)
					Terminal::out << (":");
				Terminal::out << (hex(key[i]));
			}
			Terminal::out << ("\n");

			// set key for device
			setKey(device.aesKey, Array<uint8_t const, 16>(key));
		}
		if ((extendedOptions & gp::ExtendedOptions::COUNTER_PRESENT) != 0) {
			uint32_t counter = r.u32L();
			Terminal::out << ("Counter: 0x" + hex(counter) + "\n");
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
	// nwk header (encryption header starts here)
	r.setHeader();

	auto frameControl = r.e16L<zb::NwkFrameControl>();
	uint16_t destination = r.u16L();
	uint16_t source = r.u16L();
	uint8_t radius = r.u8();
	uint8_t nwkCounter = r.u8();
	Terminal::out << (hex(destination) + " <- " + hex(source) + " (r " + dec(radius) + "); ");

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
		uint8_t relayCount = r.u8();
		uint8_t relayIndex = r.u8();
		Terminal::out << ("source route");
		for (int i = 0; i < relayCount; ++i) {
			uint16_t relay = r.u16L();
			Terminal::out << (' ' + hex(relay));
		}
		Terminal::out << ("; ");
	}

	// security header
	// note: does in-place decryption of the payload
	uint8_t const *extendedSource = nullptr;
	if ((frameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// restore security level according to 4.3.1.2 step 1.
		r.restoreSecurityLevel(securityLevel);

		// security control field (4.5.1.1)
		auto securityControl = r.e8<zb::SecurityControl>();

		// key type
		if ((securityControl & zb::SecurityControl::KEY_MASK) != zb::SecurityControl::KEY_NETWORK) {
			Terminal::out << ("Error: Only network key supported!\n");
			return;
		}

		// security counter
		uint32_t securityCounter = r.u32L();
		Terminal::out << ("SecCnt " + hex(securityCounter) + "; ");

		// extended source
		if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
			Terminal::out << ("Error: Only extended nonce supported!\n");
			return;
		}
		extendedSource = r.current;
		r.skip(8);

		// key sequence number
		uint8_t keySequenceNumber = r.u8();

		// nonce (4.5.2.2)
		Nonce nonce(extendedSource, securityCounter, securityControl);

		// decrypt in-place (whole message, mic length of 4)
		r.setMessage();
		if (!r.decrypt(4, nonce, networkAesKey)) {
			Terminal::out << "Error: NWK Decryption failed!\n";
			return;
		}
	}

	auto frameType = frameControl & zb::NwkFrameControl::TYPE_MASK;
	if (frameType == zb::NwkFrameControl::TYPE_COMMAND) {
		// nwk command
		auto command = r.e8<zb::NwkCommand>();
		switch (command) {
		case zb::NwkCommand::ROUTE_REQUEST:
			Terminal::out << "Route Request ";
			{
				auto options = r.e8<zb::NwkCommandRouteRequestOptions>();
				uint8_t routeId = r.u8();
				uint16_t destinationAddress = r.u16L();
				uint8_t cost = r.u8();

				switch (options & zb::NwkCommandRouteRequestOptions::DISCOVERY_MASK) {
				case zb::NwkCommandRouteRequestOptions::DISCOVERY_SINGLE:
					Terminal::out << (hex(destinationAddress));
					if ((options & zb::NwkCommandRouteRequestOptions::EXTENDED_DESTINATION) != 0)
						Terminal::out << " ext. dest.";
					break;
				case zb::NwkCommandRouteRequestOptions::DISCOVERY_MANY_TO_ONE_WITH_SOURCE_ROUTING:
					// https://www.digi.com/resources/documentation/Digidocs/90002002/Concepts/c_zb_many_to_one_routing.htm
					Terminal::out << "Many-to-One";
					break;
				default:
					break;
				}
			}
			Terminal::out << '\n';
			break;
		case zb::NwkCommand::ROUTE_REPLY:
			Terminal::out << "Route Reply ";
			{
				auto options = r.e8<zb::NwkCommandRouteReplyOptions>();
				uint8_t routeId = r.u8();
				uint16_t originatorAddress = r.u16L();
				uint16_t destinationAddress = r.u16L(); // "Responder" in Wireshark
				uint8_t cost = r.u8();
			}
			Terminal::out << '\n';
			break;
		case zb::NwkCommand::NETWORK_STATUS:
			Terminal::out << "Network Status\n";
			break;
		case zb::NwkCommand::LEAVE:
			Terminal::out << "Leave\n";
			break;
		case zb::NwkCommand::ROUTE_RECORD:
			// https://www.digi.com/resources/documentation/Digidocs/90001942-13/concepts/c_source_routing.htm
			Terminal::out << "Route Record:";
			{
				uint8_t relayCount = r.u8();
				for (int i = 0; i < relayCount; ++i) {
					uint16_t relay = r.u16L();
					Terminal::out << (' ' + hex(relay));
				}
			}
			Terminal::out << ("\n");
			break;
		case zb::NwkCommand::REJOIN_REQUEST:
			Terminal::out << "Rejoin Request\n";
			break;
		case zb::NwkCommand::REJOIN_RESPONSE:
			Terminal::out << "Rejoin Response\n";
			break;
		case zb::NwkCommand::LINK_STATUS:
			Terminal::out << "Link Status\n";
			break;
		default:
			Terminal::out << "Unknown NWK Command\n";
		}
	} else if (frameType == zb::NwkFrameControl::TYPE_DATA) {
		// nwk data
		handleAps(r, extendedSource);
	} else {
		Terminal::out << ("Unknown NWK Frame Type!\n");
	}
}

void handleAps(PacketReader &r, uint8_t const *extendedSource) {
	// application support layer (encryption header starts here)
	r.setHeader();

	// frame control
	auto frameControl = r.e8<zb::ApsFrameControl>();
	auto frameType = frameControl & zb::ApsFrameControl::TYPE_MASK;
	if (frameType == zb::ApsFrameControl::TYPE_COMMAND) {
		// aps command
		uint8_t apsCounter = r.u8();

		// security header
		// note: does in-place decryption of the payload
		if ((frameControl & zb::ApsFrameControl::SECURITY) != 0) {
			// restore security level according to 4.4.1.2 step 5.
			r.restoreSecurityLevel(securityLevel);

			// security control field (4.5.1.1)
			auto securityControl = r.e8<zb::SecurityControl>();

			// security counter
			uint32_t securityCounter = r.u32L();

			// nonce (4.5.2.2)
			if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
				if (extendedSource == nullptr) {
					Terminal::out << ("Error: Only extended nonce supported!\n");
					return;
				}
			} else {
				extendedSource = r.current;
				r.skip(8);
			}
			Nonce nonce(extendedSource, securityCounter, securityControl);

			// select key
			auto keyType = securityControl & zb::SecurityControl::KEY_MASK;
			AesKey const *key;
			switch (keyType) {
			case zb::SecurityControl::KEY_LINK:
				key = &linkAesKey;
				break;
			case zb::SecurityControl::KEY_KEY_TRANSPORT:
				key = &zb::za09KeyTransportAesKey;
				break;
			case zb::SecurityControl::KEY_KEY_LOAD:
				key = &zb::za09KeyLoadAesKey;
				break;
			default:
				Terminal::out << "Error: Unsupported key type in APS security header!\n";
				return;
			}

			// decrypt in-place (mic length of 4)
			r.setMessage();
			if (!r.decrypt(4, nonce, *key)) {
				Terminal::out << ("Error: APS Decryption failed!\n");
				return;
			}
		}

		auto command = r.e8<zb::ApsCommand>();
		switch (command) {
		case zb::ApsCommand::TRANSPORT_KEY:
			{
				Terminal::out << "Transport Key\n";
				auto keyType = r.e8<zb::StandardKeyType>();
				auto key = r.data8<16>();
				if (keyType == zb::StandardKeyType::NETWORK)
					uint8_t keySequenceNumber = r.u8();
				auto extendedDestination = r.data8<8>();
				auto extendedSource = r.data8<8>();

				// set key
				switch (keyType) {
				case zb::StandardKeyType::NETWORK:
					setKey(networkAesKey, key);
					break;
				case zb::StandardKeyType::TRUST_CENTER_LINK:
					setKey(linkAesKey, key);
					break;
				default:
					;
				}
			}
			break;
		case zb::ApsCommand::UPDATE_DEVICE:
			Terminal::out << "Update Device\n";
			break;
		case zb::ApsCommand::REQUEST_KEY:
			Terminal::out << "Request Key\n";
			break;
		case zb::ApsCommand::VERIFY_KEY:
			Terminal::out << "Verify Key\n";
			break;
		case zb::ApsCommand::CONFIRM_KEY:
			Terminal::out << "Confirm Key\n";
			break;
		default:
			Terminal::out << "Unknown APS Command!\n";
		}
	} else if (frameType == zb::ApsFrameControl::TYPE_DATA) {
		// aps data: zdp or zcl follow
		uint8_t destinationEndpoint = r.u8();
		if (destinationEndpoint == 0)
			handleZdp(r);
		else
			handleZcl(r, destinationEndpoint);
	} else if (frameType == zb::ApsFrameControl::TYPE_ACK) {
		// aps ack
		Terminal::out << ("Ack\n");
	} else {
		Terminal::out << ("Unknown APS Frame Type!\n");
	}
}

void handleZdp(PacketReader &r) {
	zb::ZdpCommand command = r.e16L<zb::ZdpCommand>();
	uint16_t profile = r.u16L();
	uint8_t sourceEndpoint = r.u8();
	uint8_t apsCounter = r.u8();

	switch (command) {
	case zb::ZdpCommand::NETWORK_ADDRESS_REQUEST:
		Terminal::out << ("Network Address Request\n");
		break;
	case zb::ZdpCommand::EXTENDED_ADDRESS_REQUEST:
		Terminal::out << ("Extended Address Request\n");
		break;
	case zb::ZdpCommand::EXTENDED_ADDRESS_RESPONSE:
		Terminal::out << ("Extended Address Response\n");
		break;
	case zb::ZdpCommand::NODE_DESCRIPTOR_REQUEST:
		Terminal::out << ("Node Descriptor Request\n");
		break;
	case zb::ZdpCommand::NODE_DESCRIPTOR_RESPONSE:
		Terminal::out << ("Node Descriptor Response\n");
		break;
	case zb::ZdpCommand::SIMPLE_DESCRIPTOR_REQUEST:
		Terminal::out << ("Simple Descriptor Request\n");
		break;
	case zb::ZdpCommand::SIMPLE_DESCRIPTOR_RESPONSE:
		Terminal::out << ("Simple Descriptor Response\n");
		break;
	case zb::ZdpCommand::ACTIVE_ENDPOINT_REQUEST:
		Terminal::out << ("Active Endpoint Request\n");
		break;
	case zb::ZdpCommand::ACTIVE_ENDPOINT_RESPONSE:
		Terminal::out << ("Active Endpoint Response\n");
		break;
	case zb::ZdpCommand::MATCH_DESCRIPTOR_REQUEST:
		Terminal::out << ("Match Descriptor Request\n");
		break;
	case zb::ZdpCommand::MATCH_DESCRIPTOR_RESPONSE:
		Terminal::out << ("Match Descriptor Response\n");
		break;
	case zb::ZdpCommand::DEVICE_ANNOUNCEMENT:
		Terminal::out << ("Device Announcement\n");
		break;
	case zb::ZdpCommand::BIND_REQUEST:
		Terminal::out << ("Bind Request\n");
		break;
	case zb::ZdpCommand::BIND_RESPONSE:
		Terminal::out << ("Bind Response\n");
		break;
	case zb::ZdpCommand::PERMIT_JOIN_REQUEST:
		Terminal::out << "Permit Joint Request\n";
		break;
	default:
		Terminal::out << "Unknown ZDP Command 0x" << hex(command) << "\n";
	}
}

void handleZcl(PacketReader &r, uint8_t destinationEndpoint) {
	zcl::Cluster cluster = r.e16L<zcl::Cluster>();
	zcl::Profile profile = r.e16L<zcl::Profile>();
	uint8_t sourceEndpoint = r.u8();
	uint8_t apsCounter = r.u8();

	// cluster library frame
	auto frameControl = r.e8<zcl::FrameControl>();
	auto frameType = frameControl & zcl::FrameControl::TYPE_MASK;
	bool manufacturerSpecificFlag = (frameControl & zcl::FrameControl::MANUFACTURER_SPECIFIC) != 0;
	//bool directionFlag = frameControl & 0x80; // false: client to server, true: server to client

	uint8_t zclCounter = r.u8();

	Terminal::out << ("ZclCnt " + dec(zclCounter) + "; ");

	if (frameType == zcl::FrameControl::TYPE_PROFILE_WIDE && !manufacturerSpecificFlag) {
		auto command = r.e8<zcl::Command>();
		switch (command) {
		case zcl::Command::CONFIGURE_REPORTING:
			Terminal::out << ("Configure Reporting\n");
			break;
		case zcl::Command::CONFIGURE_REPORTING_RESPONSE:
			Terminal::out << ("Configure Reporting Response\n");
			break;
		case zcl::Command::READ_ATTRIBUTES:
			Terminal::out << ("READ Attributes; ");
			switch (cluster) {
			case zcl::Cluster::BASIC:
				{
					auto attribute = r.e16L<zcl::BasicAttribute>();

					switch (attribute) {
					case zcl::BasicAttribute::MODEL_IDENTIFIER:
						Terminal::out << ("Model Identifier\n");
						break;
					default:
						Terminal::out << ("Unknown Attribute\n");
					}
				}
				break;
			case zcl::Cluster::POWER_CONFIGURATION:
				{
					auto attribute = r.e16L<zcl::PowerConfigurationAttribute>();

					switch (attribute) {
					case zcl::PowerConfigurationAttribute::BATTERY_VOLTAGE:
						Terminal::out << ("Battery Voltage\n");
						break;
					default:
						Terminal::out << ("Unknown Attribute\n");
					}
				}
				break;
			case zcl::Cluster::IDENTIFY:
				Terminal::out << ("Unknown Attribute\n");
				break;
			case zcl::Cluster::ON_OFF:
				Terminal::out << ("Unknown Attribute\n");
				break;
			default:
				Terminal::out << ("Unknown Attribute\n");
			}
			break;
		case zcl::Command::READ_ATTRIBUTES_RESPONSE:
			Terminal::out << ("READ Attributes Response; ");
			switch (cluster) {
			case zcl::Cluster::BASIC:
				{
					auto attribute = r.e16L<zcl::BasicAttribute>();
					uint8_t status = r.u8();

					if (status == 0) {
						auto dataType = r.e8<zcl::DataType>();

						switch (attribute) {
						case zcl::BasicAttribute::MODEL_IDENTIFIER:
							{
								Terminal::out << ("Model Identifier: " + r.string() + '\n');
							}
							break;
						default:
							Terminal::out << ("Unknown\n");
						}
					} else {
						Terminal::out << ("Failed\n");
					}
				}
				break;
			case zcl::Cluster::POWER_CONFIGURATION:
				{
					auto attribute = r.e16L<zcl::PowerConfigurationAttribute>();
					uint8_t status = r.u8();

					if (status == 0) {
						auto dataType = r.e8<zcl::DataType>();

						switch (attribute) {
						case zcl::PowerConfigurationAttribute::BATTERY_VOLTAGE:
							{
								int value;
								switch (dataType) {
								case zcl::DataType::UINT8:
									value = uint8_t(r.u8());
									break;
								default:
									value = 0;
								}

								Terminal::out << ("Battery Voltage " + dec(value / 10) + '.' + dec(value % 10) + "V\n");
							}
							break;
						default:
							Terminal::out << ("Unknown\n");
						}
					} else {
						Terminal::out << ("Failed\n");
					}
				}
				break;
			case zcl::Cluster::IDENTIFY:
				Terminal::out << ("Unknown Attribute\n");
				break;
			case zcl::Cluster::ON_OFF:
				Terminal::out << ("Unknown Attribute\n");
				break;
			default:
				Terminal::out << ("Unknown Attribute\n");
			}
			break;
		case zcl::Command::REPORT_ATTRIBUTES:
			Terminal::out << "Report Attributes\n";
			break;
		case zcl::Command::DEFAULT_RESPONSE:
			Terminal::out << "Default Response\n";
			break;
		default:
			Terminal::out << "Unknown ZCL Command\n";
		}
	} else if (frameType == zcl::FrameControl::TYPE_CLUSTER_SPECIFIC && !manufacturerSpecificFlag) {
		switch (cluster) {
		case zcl::Cluster::BASIC:
			Terminal::out << "Cluster: Basic\n";
			break;
		case zcl::Cluster::POWER_CONFIGURATION:
			Terminal::out << "Cluster: Power Configuration\n";
			break;
		case zcl::Cluster::ON_OFF:
			Terminal::out << "Cluster: On/Off; ";
			{
				uint8_t command = r.u8();
				switch (command) {
				case 0:
					Terminal::out << "Off\n";
					break;
				case 1:
					Terminal::out << "On\n";
					break;
				case 2:
					Terminal::out << "Toggle\n";
					break;
				default:
					Terminal::out << "Unknown Command\n";
				}
			}
			break;
		case zcl::Cluster::LEVEL_CONTROL:
			Terminal::out << "Cluster: Level Control; \n";
			break;
		case zcl::Cluster::GREEN_POWER:
			Terminal::out << "Cluster: Green Power; ";
			{
				uint8_t command = r.u8();
				switch (command) {
				case 2:
					Terminal::out << "GP Proxy Commissioning Mode\n";
					break;
				default:
					Terminal::out << "Unknown Command\n";
				}
			}
			break;
		case zcl::Cluster::COLOR_CONTROL:
			Terminal::out << "Cluster: Color Control; \n";
			break;
		default:
			Terminal::out << "Unknown Cluster 0x" << hex(cluster) << "\n";
		}
	} else {
		Terminal::out << "Unknown ZCL Frame Type\n";
	}
}


int controlTransfer(libusb_device_handle *handle, Radio::Request request, uint16_t wValue, uint16_t wIndex) {
	return libusb_control_transfer(handle,
		uint8_t(usb::Request::OUT | usb::Request::TYPE_VENDOR | usb::Request::RECIPIENT_INTERFACE),
		uint8_t(request), wValue, wIndex,
		nullptr, 0, 1000);
}

int main(int argc, char const *argv[]) {
	fs::path inputFile;
	fs::path outputFile;
	bool haveKey = false;
	int radioChannel = 15;
	auto radioFlags = Radio::ContextFlags::PASS_ALL;
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
			haveKey = true;
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
			radioFlags |= Radio::ContextFlags::HANDLE_ACK;
		} else {
			// output pcap file
			if (arg == "-o" )
				++i;
			outputFile = argv[i];
		}
	}
	if (!haveKey)
		Terminal::err << "no network key given (-k x:y:z:...)\n";

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
				Terminal::err << ("get device descriptor error: " + dec(ret) + '\n');
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
				Terminal::err << ("open error: " + dec(ret) + '\n');
				continue;
			}

			// set configuration (reset alt_setting, reset toggles)
			ret = libusb_set_configuration(handle, 1);
			if (ret != LIBUSB_SUCCESS) {
				Terminal::err << ("set configuration error: " + dec(ret) + '\n');
				continue;
			}

			// claim interface with bInterfaceNumber = 0
			ret = libusb_claim_interface(handle, 0);
			if (ret != LIBUSB_SUCCESS) {
				Terminal::err << ("claim interface error: " + dec(ret) + '\n');
				continue;
			}

			//ret = libusb_set_interface_alt_setting(handle, 0, 0);

			pcap::Header header;
			Packet packet;
			int length;

			// reset the radio
			controlTransfer(handle, Radio::Request::RESET, 0, 0);
			while (libusb_bulk_transfer(handle, 1 | usb::IN, packet.data, sizeof(packet.data), &length, 10) == LIBUSB_SUCCESS)
				;

			// configure the radio
			controlTransfer(handle, Radio::Request::START, radioChannel, 0);
			controlTransfer(handle, Radio::Request::ENABLE_RECEIVER, 1, 0);
			controlTransfer(handle, Radio::Request::SET_FLAGS, uint16_t(radioFlags), 0);

			// open output pcap file
			FILE *file = fopen(outputFile.string().c_str(), "wb");

			// write pcap header
			if (file != nullptr) {
				Terminal::out << "capturing to " << str(outputFile.string().c_str()) << "\n";
				header.magic_number = 0xa1b2c3d4;
				header.version_major = 2;
				header.version_minor = 4;
				header.thiszone = 0;
				header.sigfigs = 0;
				header.snaplen = 128;
				header.network = pcap::Network::IEEE802_15_4;
				fwrite(&header, sizeof(header), 1, file);
			}

			// loop
			Terminal::out << "waiting for IEEE 802.15.4 packets from device on channel " << dec(radioChannel) << " ...\n";
			//auto startTime = std::chrono::steady_clock::now();
			uint32_t startTime = -1;
			while (true) {
				// receive from radio
				ret = libusb_bulk_transfer(handle, 1 | usb::IN, packet.data, sizeof(packet.data), &length, 0);
				if (ret != LIBUSB_SUCCESS) {
					Terminal::err << ("transfer error: " + dec(ret) + '\n');
					break;
				}
				length -= 5;
				Terminal::out << ("length: " + dec(length) + ", LQI: " + dec(packet.data[length]) + '\n');
				uint8_t const *ts = &packet.data[length + 1];
				uint32_t timestamp = ts[0] | (ts[1] << 8) | (ts[2] << 16) | (ts[3] << 24);
				if (startTime == -1)
					startTime = timestamp;
				timestamp -= startTime;
				Terminal::out << ("timestamp: " + dec(timestamp / 1000000) + "." + dec(timestamp % 1000000) + '\n');

				if (length > 0) {
					// write to pcap file
					//auto now = std::chrono::steady_clock::now();
					//auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
					if (file != nullptr) {
						packet.header.ts_sec = timestamp / 1000000;
						packet.header.ts_usec = timestamp % 1000000;
						packet.header.incl_len = length;
						packet.header.orig_len = length + 2; // 2 byte crc not transferred
						fwrite(&packet, 1, sizeof(pcap::PacketHeader) + packet.header.incl_len, file);

						// flush file so that we can interrupt any time
						fflush(file);
					}

					// dissect
					PacketReader r(length, packet.data);
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
		std::string name = inputFile.string();
		FILE *file = fopen(name.c_str(), "rb");
		if (file == nullptr) {
			Terminal::err << "error: can't read pcap file " << str(name.c_str()) << "\n";
		} else {
			// read pcap header
			pcap::Header header;
			if (fread(&header, sizeof(header), 1, file) < 1) {
				Terminal::err << "error: pcap header incomplete!\n";
			} else if (header.network != pcap::Network::IEEE802_15_4) {
				Terminal::err << "error: protocol not supported!\n";
			} else {
				// read packets
				Packet packet;
				while (fread(&packet.header, sizeof(pcap::PacketHeader), 1, file) == 1) {
					// read packet data
					int len = min(packet.header.incl_len, int(sizeof(packet.data)));
					if (fread(packet.data, 1, len, file) < packet.header.incl_len)
						break;

					// dissect packet
					PacketReader r(packet.header.incl_len, packet.data);
					handleIeee(r);
				}
			}
			fclose(file);
		}
	}
	return 0;
}

