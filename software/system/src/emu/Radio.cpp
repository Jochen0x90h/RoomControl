#include "../Radio.hpp"
#include <usb.hpp>
#include <emu/Loop.hpp>
#include <crypt.hpp>
#include <ieee.hpp>
#include <Nonce.hpp>
#include <Queue.hpp>
#include <boardConfig.hpp>
#include <libusb.h>
#include <cstdio>


namespace Radio {

// the channel the radio receives and sends on
int channel;


// context
// -------

uint64_t longAddress;

struct Context {
	ContextFlags volatile flags;
	uint16_t volatile pan;
	uint16_t volatile shortAddress;

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;

	
	bool filter(uint8_t const *data) const {
		uint8_t const *mac = data + 1;
		ContextFlags flags = this->flags;
		
		// ALL: all packets pass
		if ((flags & ContextFlags::PASS_ALL) != 0)
			return true;		

		// get frame control field
		auto frameControl = ieee::FrameControl(mac[0] | (mac[1] << 8));
		auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;

		// reject frames with no sequence number
		if ((frameControl & ieee::FrameControl::SEQUENCE_NUMBER_SUPPRESSION) != 0)
			return false;

		// beacon packets
		if ((flags & ContextFlags::PASS_TYPE_BEACON) != 0 && frameType == ieee::FrameControl::TYPE_BEACON)
			return true;
			
		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_FLAG) != 0) {
			// has destination address: check pan
			uint16_t pan = mac[3] | (mac[4] << 8);
			if (pan != 0xffff && pan != this->pan)
				return false;
				
			if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
				// short destination addressing mode
				if ((flags & ContextFlags::PASS_DEST_SHORT) != 0
					|| ((flags & ContextFlags::PASS_TYPE_DATA_DEST_SHORT) != 0 && frameType == ieee::FrameControl::TYPE_DATA))
				{
					uint16_t shortAddress = mac[5] | (mac[6] << 8);
					if (shortAddress == 0xffff || shortAddress == this->shortAddress)
						return true;
				}
			} else {
				// long destination addressing mode
				if ((flags & ContextFlags::PASS_DEST_LONG) != 0) {
					uint32_t longAddressLo = mac[5] | (mac[6] << 8) | (mac[7] << 16) | (mac[8] << 24);
					uint32_t longAddressHi = mac[9] | (mac[10] << 8) | (mac[11] << 16) | (mac[12] << 24);
					if (longAddressLo == uint32_t(Radio::longAddress) && longAddressHi == uint32_t(Radio::longAddress >> 32))
						return true;
				}
			}
		}
		
		return false;
	}
};

Context contexts[RADIO_CONTEXT_COUNT];


// let the emulated radio receive some data
void receiveData(uint8_t *packet) {
	for (auto &context : Radio::contexts) {
		if (context.filter(packet)) {
			// resume first waiting coroutine
			context.receiveWaitlist.resumeFirst([packet](ReceiveParameters &p) {
				// length without crc but with extra data
				int length = packet[0] - 2 + Radio::RECEIVE_EXTRA_LENGTH;
				array::copy(1 + length, p.packet, packet);
				return true;
			});
		}
	}
}


// radio connected via USB
// -----------------------

libusb_device_handle *device = nullptr;


// emulated radio devices
// ----------------------

// little endian 32 bit integer
#define I32L(value) uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)

static AesKey const defaultAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

struct GreenPowerDevice {
	// device id
	uint32_t deviceId;

	// device security counter
	uint32_t securityCounter;

	// device key
	AesKey key;
	
	// last rocker state
	int lastRocker = 0;
};

GreenPowerDevice devices[1];

// data used to initialize the green power devices
struct GreenPowerDeviceData {
	uint32_t deviceId;
	uint32_t securityCounter;
	uint8_t key[16];
};

GreenPowerDeviceData const deviceData[1] = {{
	0x12345678,
	0,
	{0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf}
}};


// read from pcap file
// -------------------

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
struct PcapPacketHeader {
	uint32_t ts_sec;         // timestamp seconds
	uint32_t ts_usec;        // timestamp microseconds
	uint32_t incl_len;       // number of octets of packet saved in file
	uint32_t orig_len;       // actual length of packet
};

FILE *pcapIn = nullptr;
FILE *pcapOut = nullptr;


ReceiveParameters::ReceiveParameters(ReceiveParameters &&p) noexcept
	: WaitlistElement(std::move(p)), packet(p.packet)
{
}
	
void ReceiveParameters::add(WaitlistHead &head) noexcept {
	WaitlistElement::add(head);
}

void ReceiveParameters::remove() noexcept {
	WaitlistElement::remove();
}


SendParameters::SendParameters(SendParameters &&p) noexcept
	: WaitlistElement(std::move(p)), packet(p.packet), result(p.result)
{
}
	
void SendParameters::add(WaitlistHead &head) noexcept {
	WaitlistElement::add(head);
}

void SendParameters::remove() noexcept {
	WaitlistElement::remove();

	// send mac counter to device to cancel the send operation
	if (Radio::device != nullptr) {
		uint8_t macCounter = this->packet[3];
		int length = packet[0] - 2;
		int index = packet[1 + length]; // index is stored in send extra data
		int transferred;

		int ret = libusb_bulk_transfer(Radio::device, 1 + index | usb::OUT, &macCounter, 1, &transferred, 10000);
		if (ret == 0) {
		}
	}
}


// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle(Gui &gui) {
	for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
		auto &context = Radio::contexts[index];
	
		// resume all failed send operations (send operations fail e.g. when there is no usb radio device)
		context.sendWaitlist.resumeAll([](SendParameters &p) {
			if (p.result == 0)
				return true;
			return false;
		});
	
		while (true) {
			// check if usb device is present
			if (Radio::device != nullptr) {
				// receive buffer: length, payload, LQI
				Radio::Packet packet;

				int length;
				int ret = libusb_bulk_transfer(Radio::device, 1 + index | usb::IN, packet + 1,
					array::count(packet) - 1, &length, 1);
				if (ret != LIBUSB_SUCCESS)
					break;
				if (length == 1) {
					// got a result of an energy detection
					uint8_t ed = packet[1];
					
				} else if (length == 2) {
					// got a result of a send operation
					uint8_t macCounter = packet[1];
					uint8_t result = packet[2];

					// resume coroutine that sent the packet with matching mac counter
					context.sendWaitlist.resumeAll([macCounter, result](SendParameters &p) {
						// check if mac counter matches
						if (p.packet[3] == macCounter) {
							p.result = result;
							return true;
						}
						return false;
					});
				} else if (length >= 2 + Radio::RECEIVE_EXTRA_LENGTH) {
					// got a receive packet
					
					// resume first waiting coroutine
					uint8_t *d = packet;
					context.receiveWaitlist.resumeFirst([length, d](ReceiveParameters &p) {
						// convert to radio format where first byte is length of payload and crc, but no extra data
						p.packet[0] = length - Radio::RECEIVE_EXTRA_LENGTH + 2;
						array::copy(length, p.packet + 1, d + 1);
						return true;
					});
				}
			} else {
				break;
			}
		}
	}
	
	// call next handler in chain
	Radio::nextHandler(gui);

	// emulated devices on user interface
	gui.newLine();
	uint32_t id = 0x308419a4; // random number to identify gui components
	for (GreenPowerDevice &device : devices) {

		// commissioning can be triggered by a small commissioning button
		auto value = gui.button(id++, 0.2f);
		if (value && *value == 1) {
			uint8_t packet[] = {0, // length of packet
				0x01, 0x08, uint8_t(device.securityCounter), 0xff, 0xff, 0xff, 0xff, // mac header
				0x0c, // network header
				I32L(device.deviceId), // deviceId
				0xe0, 0x02, 0xc5, 0xf2, // command and flags
				0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, // key
				I32L(0), // mic
				I32L(device.securityCounter), // counter
				50, // LQI (extra data)
				0, 0, 0, 0}; // timestamp (extra data)
			packet[0] = array::count(packet) - 1 - 5 + 2; // + 2 for crc which is not in the packet

			// nonce
			Nonce nonce(device.deviceId, device.deviceId);

			// header is deviceId
			uint8_t const *header = packet + 9;
			int headerLength = 4;

			// payload is key
			uint8_t *message = packet + 17;

			encrypt(message, header, headerLength, message, 16, 4, nonce, defaultAesKey);

			Radio::receiveData(packet);
			++device.securityCounter;
		}

		value = gui.doubleRocker(id++);
		if (value) {
			int rocker = *value;

			uint8_t command = rocker != 0 ? 0 : 0x04;
			int r = rocker | device.lastRocker;
			if (r == 1)
				command |= 0x10;
			else if (r == 2)
				command |= 0x11;
			else if (r == 4)
				command |= 0x13;
			else if (r == 8)
				command |= 0x12;

			uint8_t packet[] = {0, // length of packet
				0x01, 0x08, uint8_t(device.securityCounter), 0xff, 0xff, 0xff, 0xff, // mac header
				0x8c, 0x30, // network header
				I32L(device.deviceId), // deviceId
				I32L(device.securityCounter), // counter
				command,
				0x00, 0x00, 0x00, 0x00, // mic
				50, // LQI (extra data)
				0, 0, 0, 0}; // timestamp (extra data)
			packet[0] = array::count(packet) - 1 - 5 + 2; // + 2 for crc which is not in the packet

			// nonce
			Nonce nonce(device.deviceId, device.securityCounter);

			// header is network header, deviceId, counter and command
			uint8_t const *header = packet + 8;
			int headerLength = 11;

			// message: empty payload and mic
			uint8_t *message = packet + 19;

			encrypt(message, header, headerLength, message, 0, 4, nonce, device.key);

			Radio::receiveData(packet);
			++device.securityCounter;
			device.lastRocker = rocker;
		}
	}
	
	// inject packets from pcap file
	if (Radio::pcapIn != nullptr) {
		PcapPacketHeader header;
		if (fread(&header, sizeof(PcapPacketHeader), 1, Radio::pcapIn) == 1) {
			// read data
			uint8_t packet[128];
			packet[0] = header.orig_len;
			int size = fread(packet + 1, 1, header.incl_len, Radio::pcapIn);
			
			// let the radio receive the packet
			Radio::receiveData(packet);
		}
	}
}

int controlTransfer(libusb_device_handle *handle, Request request, uint16_t wValue, uint16_t wIndex) {
	return libusb_control_transfer(handle,
		uint8_t(usb::Request::OUT | usb::Request::TYPE_VENDOR | usb::Request::RECIPIENT_INTERFACE),
		uint8_t(request), wValue, wIndex,
		nullptr, 0, 1000);
}

void init() {
	// check if already initialized
	if (Radio::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	Radio::nextHandler = Loop::addHandler(handle);
	
	// radio connected via USB
	int r = libusb_init(NULL);
	if (r >= 0) {
		// get list of devices
		libusb_device **devs;
		ssize_t deviceCount = libusb_get_device_list(NULL, &devs);
		if (deviceCount >= 0) {
			for (int i = 0; i < deviceCount; ++i) {
				libusb_device *dev = devs[i];
				
				// get device descriptor
				libusb_device_descriptor desc;
				int ret = libusb_get_device_descriptor(dev, &desc);
				if (ret < 0)
					continue;
				
				if (desc.idVendor == 0x1915 && desc.idProduct == 0x1337) {
					// open device
					libusb_device_handle *device;
					ret = libusb_open(dev, &device);
					if (ret != LIBUSB_SUCCESS)
						continue;
					
					// set configuration (reset alt_setting, reset toggles)
					ret = libusb_set_configuration(device, 1);
					if (ret != LIBUSB_SUCCESS)
						continue;

					// claim interface with bInterfaceNumber = 0
					ret = libusb_claim_interface(device, 0);
					if (ret != LIBUSB_SUCCESS)
						continue;

					// reset the radio
					controlTransfer(device, Request::RESET, 0, 0);

					Radio::device = device;
					
					break;
				}
			}
			libusb_free_device_list(devs, true);
			
			// check if usb device was opened
			if (Radio::device == nullptr)
				libusb_exit(NULL);
		}
	}
	
	// initialize emulated green power devices (on user interface)
	for (int i = 0; i < array::count(devices); ++i) {
		GreenPowerDeviceData const &d = deviceData[i];
		GreenPowerDevice &device = devices[i];
		//device.channel = 11; // default channel
		device.deviceId = d.deviceId;
		device.securityCounter = d.securityCounter;
		setKey(device.key, d.key);
	}


	// open input pcap file
/*	radio::pcapIn = fopen("input.pcap", "rb");
	if (radio::pcapIn != nullptr) {
		// read pcap header
		PcapHeader header;
		fread(&header, sizeof(header), 1, radio::pcapIn);
		if (header.network != 0xC3) {
			// error: protocol not supported
			radio::pcapIn = nullptr;
		}
		
		// skip all packets up to a given time
		while (true) {
			PcapPacketHeader header;
			if (fread(&header, sizeof(PcapPacketHeader), 1, radio::pcapIn) == 1) {
				// read data
				uint8_t packet[128];
				packet[0] = header.orig_len;
				fread(packet + 1, 1, header.incl_len, radio::pcapIn);
				
				if (header.ts_sec >= 25)
					break;
			} else {
				break;
			}
		}
	}

	// open output pcap file
	radio::pcapOut = fopen("output.pcap", "wb");
	if (radio::pcapOut != nullptr) {
		// write pcap header
		PcapHeader header;
		header.magic_number = 0xa1b2c3d4;
		header.version_major = 2;
		header.version_minor = 4;
		header.thiszone = 0;
		header.sigfigs = 0;
		header.snaplen = 128;
		header.network = 0xC3;
		fwrite(&header, sizeof(header), 1, radio::pcapOut);
	}
*/
}

void start(int channel) {
	assert(channel >= 10 && channel <= 26);
	Radio::channel = channel;

	if (Radio::device != nullptr)
		controlTransfer(Radio::device, Request::START, channel, 0);
}

void stop() {
	Radio::channel = -1;

	if (Radio::device != nullptr)
		controlTransfer(Radio::device, Request::STOP, 0, 0);

	// clear queues
	for (auto &context : Radio::contexts) {
		/*context.receiveWaitingQueue.resumeAll([](ReceiveParameters p) {
			// coroutines waiting for receive get a null pointer
			p.data = nullptr;
			return true;
		});*/
		context.sendWaitlist.resumeAll([](SendParameters &p) {
			// coroutines waiting for send get a failure result
			p.result = 0;
			return true;
		});
	}
}

void detectEnergy(std::function<void (uint8_t)> const &onReady) {

}

void enableReceiver(bool enable) {
	if (Radio::device != nullptr)
		controlTransfer(Radio::device, Request::ENABLE_RECEIVER, uint16_t(enable), 0);
}

void setLongAddress(uint64_t longAddress) {
	Radio::longAddress = longAddress;

	if (Radio::device != nullptr) {
		// todo: transfer to device

	}
}

uint64_t getLongAddress() {
	return Radio::longAddress;
}

void setPan(int index, uint16_t pan) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = Radio::contexts[index];

	c.pan = pan;

	if (Radio::device != nullptr)
		controlTransfer(Radio::device, Request::SET_PAN, pan, index);
}

void setShortAddress(int index, uint16_t shortAddress) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = Radio::contexts[index];

	c.shortAddress = shortAddress;

	if (Radio::device != nullptr)
		controlTransfer(Radio::device, Request::SET_SHORT_ADDRESS, shortAddress, index);
}

void setFlags(int index, ContextFlags flags) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = Radio::contexts[index];

	c.flags = flags;

	if (Radio::device != nullptr)
		controlTransfer(Radio::device, Request::SET_FLAGS, uint16_t(flags), index);
}

Awaitable<ReceiveParameters> receive(int index, Packet &packet) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	auto &context = Radio::contexts[index];

	return {context.receiveWaitlist, packet};
}

Awaitable<SendParameters> send(int index, uint8_t *packet, uint8_t &result) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	auto &context = Radio::contexts[index];

	// get length without crc but with extra data
	int length = packet[0] - 2;

	result = 1;

	// send to usb device if available
	if (Radio::device != nullptr) {
		int l = length + Radio::SEND_EXTRA_LENGTH;
		uint8_t *d = const_cast<uint8_t *>(packet + 1);
		int transferred;

		int ret = libusb_bulk_transfer(Radio::device, 1 + index | usb::OUT, d, l, &transferred, 10000);
		if (ret == 0) {
			// send zero length packet to indicate that transfer is complete if length is multiple of 64
			if (l > 0 && (l & 63) == 0)
				libusb_bulk_transfer(Radio::device, (1 + index) | usb::OUT, d, 0, &transferred, 1000);

		} else {
			// indicate failure
			result = 0;
		}
	} else {
		// indicate failure
		result = 0;
	}
		
	// store context index in extra data
	packet[1 + length] = index;
	
	// write to pcap
	if (Radio::pcapOut != nullptr) {
		// write header
		PcapPacketHeader header;
		header.ts_sec = 0;//duration / 1000000;
		header.ts_usec = 0;//duration % 1000000;
		header.incl_len = length;
		header.orig_len = length + 2; // 2 byte crc not transferred
		fwrite(&header, sizeof(header), 1, Radio::pcapOut);

		// write packet
		fwrite(packet + 1, 1, length, Radio::pcapOut);

		// flush file so that we can interrupt any time
		fflush(Radio::pcapOut);
	}
	
	return {context.sendWaitlist, packet, result};
}

} // namespace Radio
