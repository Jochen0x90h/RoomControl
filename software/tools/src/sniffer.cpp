#include <radioDefs.hpp>
#include <usbDefs.hpp>
#include <crypt.hpp>
#include <hash.hpp>
#include <Nonce.hpp>
#include <ieee.hpp>
#include <zb.hpp>
#include <gp.hpp>
#include <enum.hpp>
#include <PacketReader.hpp>
#include <util.hpp>
#include <boost/filesystem.hpp>
#include <array>
#include <string>
#include <chrono>
#include <libusb.h>
#include <stdio.h>

// logs ieee 802.15.4 traffic to a .pcap file

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
	uint8_t data[128];
};

static uint8_t const za09Key[] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};
static uint8_t const zllKey[] = {0x81, 0x42, 0x86, 0x86, 0x5D, 0xC1, 0xC8, 0xB2, 0xC8, 0xCB, 0xC5, 0x2E, 0x5D, 0x65, 0xD1, 0xB8};
AesKey trustCenterLinkKey;
AesKey trustCenterLinkKey0;
AesKey networkKey;
constexpr zb::SecurityControl securityLevel = zb::SecurityControl::LEVEL_ENC_MIC32; // encrypted + 32 bit message integrity code


void dissectZclProfileWide(PacketReader &d) {
	// cluster library frame
		
	auto command = d.enum8<zb::ZclCommand>();
	switch (command) {
	case zb::ZclCommand::CONFIGURE_REPORTING:
		printf("Configure Reporting\n");
		break;
	case zb::ZclCommand::CONFIGURE_REPORTING_RESPONSE:
		printf("Configure Reporting Response\n");
		break;
	case zb::ZclCommand::READ_ATTRIBUTES:
		printf("Read Attributes; ");
		{
			auto attribute = d.enum16<zb::ZclAttribute>();

			switch (attribute) {
			case zb::ZclAttribute::MODEL_IDENTIFIER:
				printf("Model Identifier\n");
				break;
			case zb::ZclAttribute::BATTERY_VOLTAGE:
				printf("Battery Voltage\n");
				break;
			default:
				printf("Unknown\n");
			}
		}
		break;
	case zb::ZclCommand::READ_ATTRIBUTES_RESPONSE:
		printf("Read Attributes Response; ");
		{
			auto attribute = d.enum16<zb::ZclAttribute>();
			
			uint8_t status = d.int8();
			
			if (status == 0) {
				auto dataType = d.enum8<zb::ZclDataType>();
												
				switch (attribute) {
				case zb::ZclAttribute::MODEL_IDENTIFIER:
					{
						printf("Model Identifier: %.*s\n", int(d.getRemaining()), d.current);
					}
					break;
				case zb::ZclAttribute::BATTERY_VOLTAGE:
					{
						int value;
						switch (dataType) {
						case zb::ZclDataType::UINT8:
							value = uint8_t(d.int8());
							break;
						default:
							value = 0;
						}

						printf("Battery Voltage %d.%d [V]\n", value / 10, value % 10);
					}
					break;
				default:
					printf("Unknown\n");
				}
			} else {
				printf("Failed\n");
			}
		}
		break;
	case zb::ZclCommand::DEFAULT_RESPONSE:
		printf("Default Response\n");
		break;
	default:
		printf("Unknown Command\n");
	}
}

void dissectApl(PacketReader &d) {
	// application support layer data
	uint8_t destinationEndpoint = d.int8();
	
	uint16_t cluster = d.int16();

	uint16_t profile = d.int16();

	uint8_t sourceEndpoint = d.int8();
	
	uint8_t counter = d.int8();


	if (destinationEndpoint == 0) {
		// device profile
		switch (cluster) {
		case 0x0005:
			printf("Active Endpoint Request\n");
			break;
		case 0x8005:
			printf("Active Endpoint Response\n");
			break;
		case 0x0004:
			printf("Simple Descriptor Request\n");
			break;
		case 0x8004:
			printf("Simple Descriptor Response\n");
			break;
		case 0x0021:
			printf("Bind Request\n");
			break;
		case 0x8021:
			printf("Bind Response\n");
			break;
		default:
			printf("ZDP Unknown\n");
		}
	
	} else {
		// cluster library frame
		auto frameControl = d.enum8<zb::ZclFrameControl>();
		auto frameType = frameControl & zb::ZclFrameControl::TYPE_MASK;
		bool manufacturerSpecificFlag = (frameControl & zb::ZclFrameControl::MANUFACTURER_SPECIFIC) != 0;
		//bool directionFlag = frameControl & 0x80; // false: client to server, true: server to client

		uint8_t sequenceNumber = d.int8();
		
		printf("ZCL Seq. %d; ", sequenceNumber);

		if (frameType == zb::ZclFrameControl::TYPE_PROFILE_WIDE && !manufacturerSpecificFlag) {
			dissectZclProfileWide(d);
		} else if (frameType == zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC && !manufacturerSpecificFlag) {
			switch (cluster) {
			case 0x0000:
				printf("Cluster: Basic; ");
				break;
			case 0x0001:
				printf("Cluster: Power Configuration; ");
				break;
			case 0x0006:
				printf("Cluster: On/Off; ");
				{
					uint8_t command = d.int8();
					
					switch (command) {
					case 0:
						printf("Off\n");
						break;
					case 1:
						printf("On\n");
						break;
					case 2:
						printf("Toggle\n");
						break;
					default:
						printf("Unknown Command\n");
					}
				}
				break;
			default:
				printf("Unknown Cluster\n");
			}
		} else {
			printf("Unknown Scope\n");
		}
	}
}

void dissectZb(PacketReader& d) {
	// nwk header

	// header ("string a" for decryption)
	uint8_t const *header = d.current;

	auto frameControl = d.enum16<zb::NwkFrameControl>();

	uint16_t destination = d.int16();

	uint16_t source = d.int16();

	uint8_t radius = d.int8();

	printf("%04X <- %04X (r %d); ", destination, source, radius);

	uint8_t sequenceNumber = d.int8();

	if ((frameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0) {
		uint8_t const *extendedSource = d.current;
		d.skip(8);
	}
	
	// security header
	// note: does in-place modifications of the data buffer
	if ((frameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// restore security level according to 4.3.1.2 step 1.
		d.current[0] |= securityLevel;
		
		// security control field (4.5.1.1)
		auto securityControl = d.enum8<zb::SecurityControl>();
				
		if ((securityControl & zb::SecurityControl::KEY_MASK) != zb::SecurityControl::KEY_NETWORK) {
			printf("Error: Only network key supported!\n");
			return;
		}
		if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
			printf("Error: Only extended nonce supported!\n");
			return;
		}

		uint32_t securityCounter = d.int32();
		printf("Sec. Cnt. %08x; ", securityCounter);

		uint8_t const *extendedSource = d.current;
		d.skip(8);
		
		uint8_t keySequenceNumber = d.int8();

		int headerLength = d.current - header;

		// encrypted message followed by 4 byte message integrity code (mic)
		uint8_t *message = d.current;
		int micLength = 4;
		int payloadLength = d.end - micLength - message;

		// nonce (4.5.2.2)
		Nonce nonce(extendedSource, securityCounter, securityControl);

		// in-place decrypt
		bool result = decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, networkKey);
		
		if (!result) {
			printf("Error: Decryption failed!\n");
			return;
		}
		
		d.end -= micLength;
	}
	
	auto frameType = frameControl & zb::NwkFrameControl::TYPE_MASK;
	switch (frameType) {
	case zb::NwkFrameControl::TYPE_COMMAND:
		{
			// command frame
			auto command = d.enum8<zb::NwkCommand>();
			
			switch (command) {
			case zb::NwkCommand::ROUTE_REQUEST:
				printf("Route Request\n");
				break;
			case zb::NwkCommand::LEAVE:
				printf("Leave\n");
				break;
			case zb::NwkCommand::REJOIN_REQUEST:
				printf("Rejoin Request\n");
				break;
			case zb::NwkCommand::LINK_STATUS:
				printf("Link Status\n");
				break;
			default:
				printf("Unknown NWK Command\n");
			}
		}
		break;
	case zb::NwkFrameControl::TYPE_DATA:
		// application support layer data
		{
			uint8_t *header = d.current;

			// frame control
			auto frameControl = d.enum8<zb::AplFrameControl>();
			
			auto applicationFrameType = frameControl & zb::AplFrameControl::TYPE_MASK;
			
			switch (applicationFrameType) {
			case zb::AplFrameControl::TYPE_DATA:
				dissectApl(d);
				break;
			case zb::AplFrameControl::TYPE_COMMAND:
				{
					uint8_t counter = d.int8();
					
					if ((frameControl & zb::AplFrameControl::SECURITY) != 0) {
						// restore security level according to 4.4.1.2 step 5.
						d.current[0] |= securityLevel;
						
						// security control field (4.5.1.1)
						auto securityControl = d.enum8<zb::SecurityControl>();

						if ((securityControl & zb::SecurityControl::KEY_MASK) != zb::SecurityControl::KEY_KEY_TRANSPORT) {
							printf("Error: Only key transport key supported!\n");
							return;
						}
						if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
							printf("Error: Only extended nonce supported!\n");
							return;
						}

						uint32_t frameCounter = d.int32();

						uint8_t *extendedSource = d.current;
						d.skip(8);
						
						int headerLength = d.current - header;

						// encrypted message followed by 4 byte message integrity code (mic)
						uint8_t *message = d.current;
						int micLength = 4;
						int payloadLength = d.end - micLength - message;

						// nonce (4.5.2.2)
						Nonce nonce(extendedSource, frameCounter, securityControl);

						// in-place decrypt
						bool result = decrypt(message, nonce, header, headerLength, message, payloadLength, micLength,
							trustCenterLinkKey0);
						
						if (!result) {
							printf("Error: Decryption failed!\n");
							return;
						}
						d.end -= micLength;
					}
					
					auto command = d.enum8<zb::AplCommand>();
					switch (command) {
					case zb::AplCommand::TRANSPORT_KEY:
						{
							printf("Transport Key\n");
							auto newKeyIdentifier = d.enum8<zb::KeyIdentifier>();

							uint8_t const *newKey = d.current;
							d.skip(16);
							
							uint8_t newKeySequenceNumber = d.int8();
							
							uint8_t const *extendedDestination = d.current;
							d.skip(8);
							
							uint8_t const *extendedSource = d.current;
							d.skip(8);
							
							if (newKeyIdentifier == zb::KeyIdentifier::NETWORK) {
								setKey(networkKey, newKey);
							}
						}
						break;
					}
				}
				break;
			default:
				printf("Unknown Frame Type!\n");
			}
		}
		break;
	default:
		printf("Unknown NWK Type!\n");
	}
}

// Self-powered Devices
// --------------------

struct GpDevice {
	AesKey aesKey;
};

std::map<uint32_t, GpDevice> gpDevices;

void commission(uint32_t deviceId, PacketReader& d) {
	// remove commissioning command (0xe0)
	d.int8();

	GpDevice device;

	// A.4.2.1.1 Commissioning
		
	// device type
	// 0x02: on/off switch
	auto deviceType = d.enum8<gp::DeviceType>();

	// options
	auto options = d.enum8<gp::Options>();
	if ((options & gp::Options::EXTENDED) != 0) {
		auto extendedOptions = d.enum8<gp::ExtendedOptions>();;
		
		// security level capability (used in messages)
		auto securityLevel = extendedOptions & gp::ExtendedOptions::SECURITY_LEVEL_CAPABILITY_MASK;

		if ((extendedOptions & gp::ExtendedOptions::KEY_PRESENT) != 0) {
			uint8_t *key = d.current;
			if ((extendedOptions & gp::ExtendedOptions::KEY_ENCRYPTED) != 0) {
				// Green Power A.1.5.3.3.3
				
				// nonce
				Nonce nonce(deviceId, deviceId);

				// construct a header containing the device id
				DataBuffer<4> header;
				header.setLittleEndianInt32(0, deviceId);
				
				if (!decrypt(key, nonce, header.data, 4, key, 16, 4, trustCenterLinkKey)) {
					printf("Error while decrypting key!\n");
					return;
				}
				
				// skip key and MIC
				d.skip(16 + 4);
			} else {
				// skip key
				d.skip(16);
			}

			// print key
			printf("Key: ");
			for (int i = 0; i < 16; ++i) {
				if (i > 0)
					printf(":");
				printf("%02x", key[i]);
			}
			printf("\n");

			// set key for device
			setKey(device.aesKey, key);
		}
		if ((extendedOptions & gp::ExtendedOptions::COUNTER_PRESENT) != 0) {
			uint32_t counter = d.int32();
			printf("Counter: %08x\n", counter);
		}
	}

	// check if we exceeded the end
	if (d.getRemaining() < 0)
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

void dissectGp(uint8_t const *mac, PacketReader &d) {
	// zgp stub nwk header
	
	// NWK header (frame control)
	// Security header (0, 1 or 4 byte counter)
	// NWK payload (optionally encrypted)
	// MIC (0, 2 or 4 byte message integrity code)
	uint8_t *frame = d.current;

	// frame control
	auto frameControl = d.enum8<gp::NwkFrameControl>();

	// extended frame conrol
	gp::NwkExtendedFrameControl extendedFrameControl = gp::NwkExtendedFrameControl::NONE;
	if ((frameControl & gp::NwkFrameControl::EXTENDED) != 0)
		extendedFrameControl = d.enum8<gp::NwkExtendedFrameControl>();
	auto securityLevel = extendedFrameControl & gp::NwkExtendedFrameControl::SECURITY_LEVEL_MASK;

	// device id
	uint32_t deviceId = d.int32();
	printf("Device Id: %08x; ", deviceId);

	if (securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_NONE
		&& d.peekEnum8<gp::Command>() == gp::Command::COMMISSIONING)
	{
		// commissioning (PTM215Z/216Z: hold A0 down for 7 seconds)
		commission(deviceId, d);
		return;
	}

	// get device (when the device was not commissioned, decryption will fail)
	GpDevice &device = gpDevices[deviceId];

	// security
	// --------
	// header: header that is not encrypted, payload is part of header for security levels 0 and 1
	// payload: payload that is encrypted, has zero length for security levels 0 and 1
	// mic: message integrity code, 2 or 4 bytes
	if (securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT8_MIC16) {
		// security level 1: 1 byte counter, 2 byte mic
		
		// security counter (use mac sequence number)
		uint32_t counter = mac[2];

		// nonce
		Nonce nonce(deviceId, counter);

		// header starts at mac sequence number and includes also payload
		uint8_t const *header = mac + 2;
		int micLength = 2;
		int headerLength = d.end - micLength - header;
		if (headerLength < 1)
			return;
			
		// decrypt and check
		if (!decrypt(nullptr, nonce, header, headerLength, header + headerLength, 0, micLength, device.aesKey)) {
			printf("Decrypt error; ");
		}
		
		// remove mic from end
		d.end -= 2;
	} else if (securityLevel >= gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32) {
		// security levels 2 and 3: 4 byte counter, 4 byte mic
		
		// security counter
		uint32_t counter = d.int32();

		// nonce
		Nonce nonce(deviceId, counter);

		// header starts at frame (frameControl) and either includes payload (level 2) or not (level 3)
		uint8_t *header = frame;
		int micLength = 4;
		int headerAndPayloadLength = d.end - micLength - header;
		int headerLength = securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32
			? headerAndPayloadLength : d.current - header;
		if (headerLength < 1)
			return;

		uint8_t *message = header + headerLength;
		int payloadLength = headerAndPayloadLength - headerLength;

		// decrypt in-place and check
		if (!decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, device.aesKey)) {
			if (securityLevel == 2) {
				printf("Decrypt Error; ");
			} else {
				printf("Error while decrypting message!\n");
				return;
			}
		}

		// remove mic from end
		d.end -= 4;
	}

	printf("Data:");
	int l = d.getRemaining();//end - d;
	for (int i = 0; i < l; ++i)
		printf(" %02x", d.current[i]);
	printf("\n");
}

void dissectIeee(PacketReader &d) {
	// ieee 802.15.4 mac
	// -----------------
	uint8_t const *mac = d.current;

	// frame control
	auto frameControl = d.enum16<ieee::FrameControl>();

	uint8_t sequenceNumber = 0;
	if ((frameControl & ieee::FrameControl::SEQUENCE_NUMBER_SUPPRESSION) == 0) {
		sequenceNumber = d.int8();
		printf("Seq. %d; ", int(sequenceNumber));
	}

	// destination pan/address
	uint16_t destinationPan = 0;
	bool haveDestination = (frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_FLAG) != 0;
	if (haveDestination) {
		// destination address present
		destinationPan = d.int16();

		printf("%04X:", destinationPan);
	
		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
			// short destination address
			uint16_t destination = d.int16();
			
			printf("%04X <- ", destination);
		} else {
			// long destination address
			uint64_t destination = d.int64();
			
			printf("%016llX <- ", destination); // PRIx64
		}
	}
	
	// source pan/address
	uint16_t sourcePan = destinationPan;
	if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_FLAG) != 0) {
		// source address present
		if ((frameControl & ieee::FrameControl::PAN_ID_COMPRESSION) == 0 || !haveDestination) {
			sourcePan = d.int16();
		}
		
		printf("%04X:", sourcePan);
	
		if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0) {
			// short source address
			uint16_t source = d.int16();
			
			printf("%04X; ", source);
		} else {
			// long source address
			uint64_t source = d.int64();
			
			printf("%016llX; ", source);
		}
	}
	
	auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;
	switch (frameType) {
	case ieee::FrameControl::TYPE_BEACON:
		{
			uint16_t superframeSpecification = d.int16();
			
			uint8_t gts = d.int8();
			
			uint8_t pending = d.int8();
			
			uint8_t protocolId = d.int8();
			
			uint16_t stackProfile = d.int16();
			int type = stackProfile & 15;
			int protocolVersion = (stackProfile >> 4) & 15;
			bool routerFlag = stackProfile & 0x400;
			
			printf("Beacon: %d, %d", type, protocolVersion);
			if (routerFlag)
				printf(", router");
			printf("\n");
		}
		break;
	case ieee::FrameControl::TYPE_COMMAND:
		{
			auto command = d.enum8<ieee::Command>();
			
			switch (command) {
			case ieee::Command::ASSOCIATION_REQUEST:
				printf("Association Request\n");
				break;
			case ieee::Command::ASSOCIATION_RESPONSE:
				printf("Association Response\n");
				break;
			case ieee::Command::DATA_REQUEST:
				printf("Data Request\n");
				break;
			case ieee::Command::BEACON_REQUEST:
				printf("Beacon Request\n");
				break;
			default:
				printf("Unknown Command\n");
			}
		}
		break;
	case ieee::FrameControl::TYPE_ACK:
		printf("Ack\n");
		break;
	case ieee::FrameControl::TYPE_DATA:
		{
			auto version = d.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
			switch (version) {
			case gp::NwkFrameControl::VERSION_2:
				dissectZb(d);
				break;
			case gp::NwkFrameControl::VERSION_3_GP:
				dissectGp(mac, d);
				break;
			default:
				printf("Unknown Nwk Frame Version!\n");
			}
		}
		break;
	default:
		printf("Unknown IEEE Frame Type\n");
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
	
	setKey(trustCenterLinkKey, za09Key);

	DataBuffer<16> hashedKey;
	keyHash(hashedKey, za09Key, 0);
	setKey(trustCenterLinkKey0, hashedKey);
	
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
			
			setKey(networkKey, key);
		} else if (arg == "-i" || arg == "--input") {
			// input pcap file
			++i;
			inputFile = argv[i];
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
				fprintf(stderr, "get device descriptor error: %d\n", ret);
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
				fprintf(stderr, "open error: %d\n", ret);
				continue;
			}
				
			// set configuration (reset alt_setting, reset toggles)
			ret = libusb_set_configuration(handle, 1);
			if (ret != LIBUSB_SUCCESS) {
				fprintf(stderr, "set configuration error: %d\n", ret);
				continue;
			}

			// claim interface with bInterfaceNumber = 0
			ret = libusb_claim_interface(handle, 0);
			if (ret != LIBUSB_SUCCESS) {
				fprintf(stderr, "claim interface error: %d\n", ret);
				continue;
			}

			//ret = libusb_set_interface_alt_setting(handle, 0, 0);

			// configure the radio
			//controlTransfer(handle, radio::Request::RESET, 0, 0);
			//controlTransfer(handle, radio::Request::START, 15, 0);
			//controlTransfer(handle, radio::Request::ENABLE_RECEIVER, 1, 0);
			controlTransfer(handle, radio::Request::SET_FLAGS, uint16_t(radioFlags), 0);

			// open output pcap file
			FILE *pcap = fopen(outputFile.string().c_str(), "wb");
			
			// write pcap header
			PcapHeader header;
			header.magic_number = 0xa1b2c3d4;
			header.version_major = 2;
			header.version_minor = 4;
			header.thiszone = 0;
			header.sigfigs = 0;
			header.snaplen = 128;
			header.network = 0xC3;
			fwrite(&header, sizeof(header), 1, pcap);

			// loop
			printf("waiting for ieee802.15.4 packet from device ...\n");
			PcapPacket packet;
			int length;
			auto startTime = std::chrono::steady_clock::now();
			while (true) {
				std::fill(std::begin(packet.data), std::end(packet.data), 0xAA);
				
				// receive from radio
				ret = libusb_bulk_transfer(handle, 1 | usb::IN, packet.data, 256, &length, 0);
				if (ret != LIBUSB_SUCCESS) {
					fprintf(stderr, "transfer error: %d\n", ret);
					break;
				}
				--length;
				printf("length: %d, LQI: %d\n", length, packet.data[length]);
				
				auto time = std::chrono::steady_clock::now();
				auto duration = std::chrono::duration_cast<std::chrono::microseconds>(time - startTime).count();
				packet.ts_sec = duration / 1000000;
				packet.ts_usec = duration % 1000000;
				packet.incl_len = length;
				packet.orig_len = length + 2; // 2 byte crc not transferred
				fwrite(&packet, offsetof(PcapPacket, data) + packet.incl_len, 1, pcap);
				
				// flush file so that we can interrupt
				fflush(pcap);
				
				PacketReader d(packet.data, packet.data + packet.incl_len);
				dissectIeee(d);
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
			printf("error: protocol not supported!\n");
		}

		// read packets
		PcapPacket packet;
		while (fread(&packet, offsetof(PcapPacket, data), 1, pcap) == 1) {
			// read data
			fread(packet.data, packet.incl_len, 1, pcap);
			
			PacketReader d(packet.data, packet.data + packet.incl_len);
			dissectIeee(d);
		}
	}
	return 0;
}

