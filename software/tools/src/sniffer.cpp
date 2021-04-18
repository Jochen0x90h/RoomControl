#include <stdio.h>
#include <libusb.h>
#include <util.hpp>
#include <array>
#include <string>
#include <crypt.hpp>
#include <hash.hpp>
#include <boost/filesystem.hpp>

// logs ieee 802.15.4 traffic to a .pcap file


namespace fs = boost::filesystem;

// transfer direction
enum UsbDirection {
	USB_OUT = 0, // to device
	USB_IN = 0x80 // to host
};

// header of pcap file
struct PcapHeader {
	uint32_t magic_number;   // magic number
	uint16_t version_major;  // major version number
	uint16_t version_minor;  // minor version number
	int32_t  thiszone;       // GMT to local correction
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


enum class IeeeFrameType {
	BEACON = 0,
	DATA = 1,
	ACK = 2,
	COMMAND = 3
};

enum class IeeeAddressingMode {
	NONE = 0,
	SHORT = 2,
	LONG = 3
};

enum class IeeeCommand {
	ASSOCIATION_REQUEST = 1,
	ASSOCIATION_RESPONSE = 2,
	DATA_REQUEST = 4,
	BEACON_REQUEST = 7,
};

enum class ZigbeeFrameType {
	DATA = 0,
	COMMAND = 1,
};

enum class ZigbeeDiscoverRoute {
	ENABLE = 1,
};

// key type (table 4.31)
enum class ZigbeeKeyIdentifier {
	DATA = 0,
	NETWORK = 1,
	KEY_TRANSPORT = 2,
	KEY_LOAD = 3
};

enum class ZigbeeCommand {
	LEAVE = 4,
//	TRANSPORT_KEY = 5,
	LINK_STATUS = 8
};

enum class ZigbeeApplicationFrameType {
	DATA = 0,
	COMMAND = 1
};

enum class ZigbeeApplicationCommand {
	TRANSPORT_KEY = 5,
};

enum class ZigbeeClusterFrameType {
	PROFILE_WIDE = 0,
	CLUSTER_SPECIFIC = 1,
};

enum class ZigbeeClusterCommand {
	READ_ATTRIBUTES = 0,
	READ_ATTRIBUTES_RESPONSE = 1,
	CONFIGURE_REPORTING = 6,
	CONFIGURE_REPORTING_RESPONSE = 7,
	DEFAULT_RESPONSE = 0x0b
};

enum class ZigbeeClusterAttribute {
	MODEL_IDENTIFIER = 0x0005,
	BATTERY_VOLTAGE = 0x0020,
};

enum class ZibgeeClusterDataType {
	UINT8 = 0x20,
	STRING = 0x42
};

static uint8_t const zigbeeAlliance09Key[] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};
static uint8_t const zllKey[] = {0x81, 0x42, 0x86, 0x86, 0x5D, 0xC1, 0xC8, 0xB2, 0xC8, 0xCB, 0xC5, 0x2E, 0x5D, 0x65, 0xD1, 0xB8};
AesKey defaultAesKey;
AesKey aesKey;
constexpr uint8_t securityLevel = 5; // encrypted + 32 bit message integrity code


void dissectZigbeeClusterProfileWide(uint8_t *d, uint8_t const *end) {
	// zigbee cluster library frame
		
	ZigbeeClusterCommand command = ZigbeeClusterCommand(d[0]);
	++d;
	switch (command) {
	case ZigbeeClusterCommand::CONFIGURE_REPORTING:
		printf("Configure Reporting\n");
		break;
	case ZigbeeClusterCommand::CONFIGURE_REPORTING_RESPONSE:
		printf("Configure Reporting Response\n");
		break;
	case ZigbeeClusterCommand::READ_ATTRIBUTES:
		printf("Read Attributes; ");
		{
			ZigbeeClusterAttribute attribute = ZigbeeClusterAttribute(d[0] | (d[1] << 8));
			d += 2;

			switch (attribute) {
			case ZigbeeClusterAttribute::MODEL_IDENTIFIER:
				printf("Model Identifier\n");
				break;
			case ZigbeeClusterAttribute::BATTERY_VOLTAGE:
				printf("Battery Voltage\n");
				break;
			default:
				printf("Unknown\n");
			}
		}
		break;
	case ZigbeeClusterCommand::READ_ATTRIBUTES_RESPONSE:
		printf("Read Attributes Response; ");
		{
			ZigbeeClusterAttribute attribute = ZigbeeClusterAttribute(d[0] | (d[1] << 8));
			d += 2;
			
			uint8_t status = d[0];
			++d;
			
			if (status == 0) {
				ZibgeeClusterDataType dataType = ZibgeeClusterDataType(d[0]);
				++d;
												
				switch (attribute) {
				case ZigbeeClusterAttribute::MODEL_IDENTIFIER:
					{
						printf("Model Identifier: %.*s\n", int(end - d), d);
					}
					break;
				case ZigbeeClusterAttribute::BATTERY_VOLTAGE:
					{
						int value;
						switch (dataType) {
						case ZibgeeClusterDataType::UINT8:
							value = d[0];
							++d;
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
	case ZigbeeClusterCommand::DEFAULT_RESPONSE:
		printf("Default Response\n");
		break;
	default:
		printf("Unknown Command\n");
	}
}

void dissectZigbeeApplicationData(uint8_t *d, uint8_t const *end) {
	// zigbee application support layer data
	uint8_t destinationEndpoint = d[0];
	++d;
	
	uint16_t cluster = d[0] | (d[1] << 8);
	d += 2;

	uint16_t profile = d[0] | (d[1] << 8);
	d += 2;

	uint8_t sourceEndpoint = d[0];
	++d;
	
	uint8_t counter = d[0];
	++d;


	if (destinationEndpoint == 0) {
		// zigbee device profile
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
		uint8_t frameControl = d[0];
		++d;
		ZigbeeClusterFrameType frameType = ZigbeeClusterFrameType(frameControl & 3);
		bool manufacturerSpecificFlag = frameControl & 0x04;
		bool directionFlag = frameControl & 0x80; // false: client to server, true: server to client

		uint8_t sequenceNumber = d[0];
		++d;
		
		printf("Sequence %d; ", sequenceNumber);

		if (frameType == ZigbeeClusterFrameType::PROFILE_WIDE && !manufacturerSpecificFlag) {
			dissectZigbeeClusterProfileWide(d, end);
		} else if (frameType == ZigbeeClusterFrameType::CLUSTER_SPECIFIC && !manufacturerSpecificFlag) {
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
					uint8_t command = d[0];
					++d;
					
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

void dissectZigbee(uint8_t *d, uint8_t const *end) {

	// zigbee network layer (nwk header)

	// header ("string a" for decryption)
	uint8_t const *header = d;

	uint16_t frameControl = d[0] | (d[1] << 8);
	d += 2;
	ZigbeeFrameType frameType = ZigbeeFrameType(frameControl & 3);
	int version = (frameControl >> 2) & 15;
	ZigbeeDiscoverRoute discoverRouter = ZigbeeDiscoverRoute((frameControl >> 6) & 3);
	bool multicastFlag = (frameControl >> 8) & 1;
	bool securityFlag = (frameControl >> 9) & 1;
	bool sourceRouteFlag = (frameControl >> 10) & 1;
	bool destinationFlag = (frameControl >> 11) & 1;
	bool extendedSourceFlag = (frameControl >> 12) & 1;
	bool endDeviceInitiatorFlag = (frameControl >> 13) & 1;

	uint16_t destination = d[0] | (d[1] << 8);
	d += 2;

	uint16_t source = d[0] | (d[1] << 8);
	d += 2;

	uint8_t radius = d[0];
	++d;

	printf("%04X -> %04X (%d); ", source, destination, radius);

	uint8_t sequenceNumber = d[0];
	++d;

	if (extendedSourceFlag) {
		uint8_t const *extendedSource = d;
		d += 8;
	}
	
	// zigbee security header
	// note: does in-place modifications of the data buffer
	if (securityFlag) {
		// restore security level according to 4.3.1.2 step 1.
		d[0] |= securityLevel;
		
		// security control field (4.5.1.1)
		uint8_t securityControl = d[0];
		++d;
		ZigbeeKeyIdentifier keyIdentifier = ZigbeeKeyIdentifier((securityControl >> 3) & 3);
		bool extendedNonceFlag = securityControl & 0x20;
		if (keyIdentifier != ZigbeeKeyIdentifier::NETWORK) {
			printf("Error: Wrong key type!\n");
			return;
		}

		uint32_t frameCounter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
		d += 4;
		
		uint8_t const *extendedSource = d;
		d += 8;
		
		uint8_t keySequenceNumber = d[0];
		++d;

		int headerLength = d - header;

		// encrypted message followed by 4 byte message integrity code (mic)
		uint8_t const *message = d;
		int micLength = 4;
		int payloadLength = end - micLength - message;

		// nonce (4.5.2.2)
		DataBuffer<13> nonce;
		nonce.setData(0, extendedSource, 8);
		nonce.setLittleEndianInt32(8, frameCounter);
		nonce.setInt8(12, securityControl);

		bool result = decrypt(d, nonce, header, headerLength, message, payloadLength, micLength, aesKey);
		
		if (!result) {
			printf("Error: Decryption failed!\n");
			return;
		}
		
		end -= micLength;
	}
	
	switch (frameType) {
	case ZigbeeFrameType::COMMAND:
		{
			// command frame
			ZigbeeCommand command = ZigbeeCommand(d[0]);
			++d;
			
			switch (command) {
			case ZigbeeCommand::LEAVE:
				printf("Leave\n");
				break;
			case ZigbeeCommand::LINK_STATUS:
				printf("Link Status\n");
				break;
			default:
				printf("Unknown Command\n");
			}
		}
		break;
	case ZigbeeFrameType::DATA:
		// zigbee application support layer data
		{
			uint8_t *header = d;

			// frame control
			uint8_t frameControl = d[0];
			++d;
			
			ZigbeeApplicationFrameType applicationFrameType = ZigbeeApplicationFrameType(frameControl & 3);
			bool securityFlag = frameControl & 0x20;
			
			switch (applicationFrameType) {
			case ZigbeeApplicationFrameType::DATA:
				dissectZigbeeApplicationData(d, end);
				break;
			case ZigbeeApplicationFrameType::COMMAND:
				{
					uint8_t counter = d[0];
					++d;
					
					if (securityFlag) {
						// restore security level according to 4.4.1.2 step 5.
						d[0] |= securityLevel;
						
						// security control field (4.5.1.1)
						uint8_t securityControl = d[0];
						++d;
						ZigbeeKeyIdentifier keyIdentifier = ZigbeeKeyIdentifier((securityControl >> 3) & 3);
						bool extendedNonceFlag = securityControl & 0x20;
						if (keyIdentifier != ZigbeeKeyIdentifier::KEY_TRANSPORT) {
							printf("Error: Wrong key type!\n");
							return;
						}

						uint32_t frameCounter = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
						d += 4;
					
						uint8_t *extendedSource = d;
						d += 8;
						
						int headerLength = d - header;

						// encrypted message followed by 4 byte message integrity code (mic)
						uint8_t const *message = d;
						int micLength = 4;
						int payloadLength = end - micLength - message;

						// nonce (4.5.2.2)
						DataBuffer<13> nonce;
						nonce.setData(0, extendedSource, 8);
						nonce.setLittleEndianInt32(8, frameCounter);
						nonce.setInt8(12, securityControl);

						bool result = decrypt(d, nonce, header, headerLength, message, payloadLength, micLength,
							defaultAesKey);
						
						if (!result) {
							printf("Error: Decryption failed!\n");
							return;
						}
						end -= micLength;
					}
					
					ZigbeeApplicationCommand command = ZigbeeApplicationCommand(d[0]);
					++d;
					switch (command) {
					case ZigbeeApplicationCommand::TRANSPORT_KEY:
						{
							printf("Transport Key\n");
							ZigbeeKeyIdentifier newKeyIdentifier = ZigbeeKeyIdentifier(d[0]);
							++d;

							uint8_t const *newKey = d;
							d += 16;
							
							uint8_t newKeySequenceNumber = d[0];
							++d;
							
							uint8_t const *extendedDestination = d;
							d += 8;
							
							uint8_t const *extendedSource = d;
							d += 8;
							
							if (newKeyIdentifier == ZigbeeKeyIdentifier::NETWORK) {
								DataBuffer<16> defaultKey;
								keyHash(defaultKey, newKey, 0);
								setKey(defaultAesKey, defaultKey);
							}
						}
						break;
					}
				}
				break;
			};

		}
		
		break;
	}
}

void dissectIeee(uint8_t *d, uint8_t const *end) {
	// ieee 802.15.4 frame

	// frame control
	uint16_t frameControl = d[0] | (d[1] << 8);
	IeeeFrameType frameType = IeeeFrameType(frameControl & 7);
	d += 2;
	
	// flags
	bool securityFlag = (frameControl >> 3) & 1;
	bool framePendingFlag = (frameControl >> 4) & 1;
	bool acknowledgeRequestFlag = (frameControl >> 5) & 1;
	bool panIdCompressionFlag = (frameControl >> 6) & 1;
	bool sequenceNumberSuppressionFlag = (frameControl >> 8) & 1;
	bool informationElementsPresentFlag = (frameControl >> 9) & 1;
	IeeeAddressingMode destinationAddressingMode = IeeeAddressingMode((frameControl >> 10) & 3);
	int frameVersion = (frameControl >> 12) & 2;
	IeeeAddressingMode sourceAddressingMode = IeeeAddressingMode((frameControl >> 14) & 3);
	
	uint8_t sequenceNumber = 0;
	if (!sequenceNumberSuppressionFlag) {
		sequenceNumber = d[0];
		++d;
	}
	
	uint16_t destinationPan = 0;
	if (destinationAddressingMode != IeeeAddressingMode::NONE) {
		destinationPan = d[0] | (d[1] << 8);
		d += 2;

		printf("%04X:", destinationPan);
	}

	if (destinationAddressingMode == IeeeAddressingMode::SHORT) {
		uint16_t destination = d[0] | (d[1] << 8);
		d += 2;
		
		printf("%04X <- ", destination);
	} else if (destinationAddressingMode == IeeeAddressingMode::LONG) {
		uint32_t destinationL = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
		uint32_t destinationH = d[4] | (d[5] << 8) | (d[6] << 16) | (d[7] << 24);
		d += 8;
		
		printf("%08X%08X <- ", destinationH, destinationL);
	}
	
	uint16_t sourcePan = destinationPan;
	if (sourceAddressingMode != IeeeAddressingMode::NONE) {
		if (!panIdCompressionFlag || destinationAddressingMode == IeeeAddressingMode::NONE) {
			sourcePan = d[0] | (d[1] << 8);
			d += 2;
		}
		
		printf("%04X:", sourcePan);
	}

	if (sourceAddressingMode == IeeeAddressingMode::SHORT) {
		uint16_t source = d[0] | (d[1] << 8);
		d += 2;
		
		printf("%04X; ", source);
	} else if (sourceAddressingMode == IeeeAddressingMode::LONG) {
	   uint32_t sourceL = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
	   uint32_t sourceH = d[4] | (d[5] << 8) | (d[6] << 16) | (d[7] << 24);
	   d += 8;
	   
	   printf("%08X%08X; ", sourceH, sourceL);
	}
	

	switch (frameType) {
	case IeeeFrameType::BEACON:
		{
			uint16_t superframeSpecification = d[0] | (d[1] << 8);
			d += 2;
			
			uint8_t gts = d[0];
			++d;
			
			uint8_t pending = d[0];
			++d;
			
			uint8_t protocolId = d[0];
			++d;
			
			uint16_t stackProfile = d[0] | (d[1] << 8);
			d += 2;
			int type = stackProfile & 15;
			int protocolVersion = (stackProfile >> 4) & 15;
			bool routerFlag = stackProfile & 0x400;
			
			printf("Beacon: %d, %d", type, protocolVersion);
			if (routerFlag)
				printf(", router");
			printf("\n");
		}
		break;
	case IeeeFrameType::COMMAND:
		{
			IeeeCommand command = IeeeCommand(d[0]);
			++d;
			
			switch (command) {
			case IeeeCommand::ASSOCIATION_REQUEST:
				printf("Association Request\n");
				break;
			case IeeeCommand::ASSOCIATION_RESPONSE:
				printf("Association Response\n");
				break;
			case IeeeCommand::DATA_REQUEST:
				printf("Data Request\n");
				break;
			case IeeeCommand::BEACON_REQUEST:
				printf("Beacon Request\n");
				break;
			default:
				printf("Unknown Command\n");
			}
		}
		break;
	case IeeeFrameType::ACK:
		printf("Ack\n");
		break;
	case IeeeFrameType::DATA:
		dissectZigbee(d, end);
		break;
	}
}


int main(int argc, char const *argv[]) {
	fs::path inputFile;
	
	DataBuffer<16> defaultKey;
	keyHash(defaultKey, zigbeeAlliance09Key, 0);
	setKey(defaultAesKey, defaultKey);
	
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		
		if (arg == "-k" || arg == "--key") {
			// get network key from argument
			++i;
			int key[16];
			sscanf(argv[i], "%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
				&key[0], &key[1], &key[2], &key[3], &key[4], &key[5], &key[6], &key[7],
				&key[8], &key[9], &key[10], &key[11], &key[12], &key[13], &key[14], &key[15]);
			
			uint8_t k[16];
			for (int i = 0; i < 16; ++i)
				k[i] = key[i];
			
			setKey(aesKey, k);
		} else if (arg == "-i" || arg == "--input") {
			// input file
			++i;
			inputFile = argv[i];
		} else {
			// output pcap file
			
		}
	}

	// either read from input .pcap file or from usb device
	if (inputFile.empty()) {
		libusb_device **devs;
		int r;
		ssize_t cnt;

		r = libusb_init(NULL);
		if (r < 0)
			return r;

		cnt = libusb_get_device_list(NULL, &devs);
		if (cnt < 0){
			libusb_exit(NULL);
			return (int) cnt;
		}

		for (int i = 0; devs[i]; ++i) {
			libusb_device *dev = devs[i];
			libusb_device_descriptor desc;

			int ret = libusb_get_device_descriptor(dev, &desc);
			if (ret < 0) {
				fprintf(stderr, "failed to get device descriptor");
				return -1;
			}
			if (desc.idVendor == 0x1915 && desc.idProduct == 0x1337) {
				libusb_device_handle *handle;
				ret = libusb_open(dev, &handle);
				if (ret == LIBUSB_SUCCESS) {
					// set configuration (reset alt_setting, reset toggles)
					libusb_set_configuration(handle, 1);

					// claim interface with bInterfaceNumber = 0
					ret = libusb_claim_interface(handle, 0);
					//printf("claim interface %d\n", ret);

					//ret = libusb_set_interface_alt_setting(handle, 0, 0);
					//printf("set alternate setting %d\n", ret);

					// open pcap file
					FILE *pcap = fopen("log2.pcap", "wb");
					
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
					while (true) {
						std::fill(std::begin(packet.data), std::end(packet.data), 0xAA);
						
						// receive from device (we get back the same data that we sent)
						ret = libusb_bulk_transfer(handle, USB_IN | 1, packet.data, 256, &length, 0);
						if (ret < 0)
							break;
						printf("length: %d\n", length);
						
						packet.incl_len = length;
						packet.orig_len = length + 2; // 2 byte crc not transferred
						fwrite(&packet, offsetof(PcapPacket, data) + packet.incl_len, 1, pcap);
						
						// flush file so that we can interrupt
						fflush(pcap);
						
						dissectIeee(packet.data, packet.data + packet.incl_len);
					}
					printf("error: %d\n", ret);
				}
			}
		}
		libusb_free_device_list(devs, 1);
		libusb_exit(NULL);
	} else {
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
			
			dissectIeee(packet.data, packet.data + packet.incl_len);
		}
	}
	return 0;
}

