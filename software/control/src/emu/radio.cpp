#include <radio.hpp>
#include <crypt.hpp>
#include <emu/global.hpp>
#include "Gui.hpp"
#include <config.hpp>
#include <libusb.h>
#include <util.hpp>


namespace radio {

// the channel the radio receives and sends on
int channel = 15;


// context
// -------

uint32_t longAddressLo;
uint32_t longAddressHi;

enum class FrameType {
	BEACON = 0,
	DATA = 1,
	ACK = 2,
	COMMAND = 3
};

struct Context {
	uint16_t volatile flags;
	uint16_t volatile pan;
	uint16_t volatile shortAddress;

	// receive handler
	std::function<void (uint8_t *)> onReceived;
	
	bool filter(uint8_t const *data) const {
		uint8_t const *mac = data + 1;
		int flags = this->flags;
		
		// ALL: all packets pass
		if (flags & radio::ALL)
			return true;
		
		// reject frames with no sequence number
		if (mac[1] & 1)
			return false;
		
		// get frame type
		FrameType frameType = FrameType(mac[0] & 0x07);

		// beacon packets
		if ((flags & radio::TYPE_BEACON) && frameType == FrameType::BEACON)
			return true;
			
		if (mac[1] & 0x08) {
			// has destination address: check pan
			uint16_t pan = mac[3] | (mac[4] << 8);
			if (pan != 0xffff && pan != this->pan)
				return false;
				
			if (mac[1] & 0x04) {
				// long destination addressing mode
				if (flags & radio::DEST_LONG) {
					uint32_t longAddressLo = mac[5] | (mac[6] << 8) | (mac[7] << 16) | (mac[8] << 24);
					uint32_t longAddressHi = mac[9] | (mac[10] << 8) | (mac[11] << 16) | (mac[12] << 24);
					if (longAddressLo == radio::longAddressLo && longAddressHi == radio::longAddressHi)
						return true;
				}
			} else {
				// short destination addressing mode
				if ((flags & radio::DEST_SHORT) || ((flags & radio::TYPE_DATA_DEST_SHORT) && frameType == FrameType::DATA)) {
					uint16_t shortAddress = mac[5] | (mac[6] << 8);
					if (shortAddress == 0xffff || shortAddress == this->shortAddress)
						return true;
				}
			}
		}
		
		return false;
	}
};

Context contexts[RADIO_CONTEXT_COUNT];


// sender
// ------

std::function<void (bool)> onSent;



// radio connected via USB
// -----------------------

// transfer direction
enum UsbDirection {
	USB_OUT = 0, // to device
	USB_IN = 0x80 // to host
};

libusb_device_handle *handle = nullptr;



// emulated radio devices on user interface
// ----------------------------------------

// little endian 32 bit integer
#define LE_INT32(value) uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)

static AesKey const defaultAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

struct GreenPowerDevice {
	// the emulated radio channel the device sends on
	int channel;
	
	// device id
	uint32_t deviceId;

	// device security counter
	uint32_t counter;

	// device key
	AesKey key;
	
	// time for long button press
	std::chrono::steady_clock::time_point time;
	
	// last rocker state
	int lastRocker = 0;
};

GreenPowerDevice devices[1];

// data used to initialize the green power devices
struct GreenPowerDeviceData {
	uint32_t deviceId;
	uint32_t counter;
	uint8_t key[16];
};

GreenPowerDeviceData const deviceData[1] = {{
	0x12345678,
	0xfffffff0,
	{0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf}}};


void doGui(Gui &gui, int &id) {
	
	// radio connected via USB
	if (radio::handle != nullptr) {
		uint8_t data[128];
		
		// receive from radio
		int length;
		int ret = libusb_bulk_transfer(radio::handle, USB_IN | 1, data + 1, 127, &length, 1);
		if (ret == LIBUSB_SUCCESS) {
			// convert to radio format where first bye is length of payload including crc
			data[0] = length + 2;

			// call receive handlers
			for (auto const &c : radio::contexts) {
				if (c.onReceived)
					c.onReceived(data);
			}
		}
	}
	
	// emulated devices on user interface
	for (GreenPowerDevice &device : devices) {
		int rocker = gui.doubleRocker(id++);
		if (rocker != -1) {

			// time difference
			auto now = std::chrono::steady_clock::now();
			int ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - device.time).count();
			device.time = now;

			if (ms > 3000 && rocker == 0) {
				// released after some time: commission
				if (device.lastRocker & 1)
					device.channel = 15;
				else if (device.lastRocker & 2)
					device.channel = 20;
				else if (device.lastRocker & 4)
					device.channel = 11;
				else if (device.lastRocker & 8)
					device.channel = 25;

				// check if the emulated device is on the current radio channel
				if (device.channel == radio::channel) {
					uint8_t data[] = {0,
						0x01, 0x08, uint8_t(device.counter), 0xff, 0xff, 0xff, 0xff, // mac header
						0x0c, // network header
						LE_INT32(device.deviceId), // deviceId
						0xe0, 0x02, 0xc5, 0xf2, // command and flags
						0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, // key
						LE_INT32(0), // mic
						LE_INT32(device.counter)}; // counter
					data[0] = array::size(data) - 1 + 2;

					// nonce
					Nonce nonce(device.deviceId, device.deviceId);

					// header is deviceId
					uint8_t const *header = data + 9;
					int headerLength = 4;
					
					// payload is key
					uint8_t *message = data + 17;
					
					encrypt(message, nonce, header, headerLength, message, 16, 4, defaultAesKey);

					// call receive handlers
					for (auto const &c : radio::contexts) {
						if (c.onReceived && c.filter(data))
							c.onReceived(data);
					}
				}
			} else if (device.channel == radio::channel) {
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

				uint8_t data[] = {0,
					0x01, 0x08, uint8_t(device.counter), 0xff, 0xff, 0xff, 0xff, // mac header
					0x8c, 0x30, // network header
					LE_INT32(device.deviceId), // deviceId
					LE_INT32(device.counter), // counter
					command,
					0x00, 0x00, 0x00, 0x00}; // mic
				data[0] = array::size(data) - 1 + 2;
				
				// nonce
				Nonce nonce(device.deviceId, device.counter);

				// header is network header, deviceId, counter and command
				uint8_t const *header = data + 8;
				int headerLength = 11;

				// message: empty payload and mic
				uint8_t *message = data + 19;

				encrypt(message, nonce, header, headerLength, message, 0, 4, device.key);

				// call receive handlers
				for (auto const &c : radio::contexts) {
					if (c.onReceived && c.filter(data))
						c.onReceived(data);
				}
			}
			++device.counter;
			device.lastRocker = rocker;
		}
	}
	
	gui.newLine();
}

void initUsb() {
	int r = libusb_init(NULL);
	if (r < 0)
		return;

	libusb_device **devs;
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) {
		libusb_exit(NULL);
		return;
	}

	for (int i = 0; devs[i]; ++i) {
		libusb_device *dev = devs[i];
		
		// get device descriptor
		libusb_device_descriptor desc;
		int ret = libusb_get_device_descriptor(dev, &desc);
		if (ret < 0)
			continue;
		
		if (desc.idVendor == 0x1915 && desc.idProduct == 0x1337) {
			// open device
			libusb_device_handle *handle;
			ret = libusb_open(dev, &handle);
			if (ret != LIBUSB_SUCCESS)
				return;
			
			// set configuration (reset alt_setting, reset toggles)
			ret = libusb_set_configuration(handle, 1);
			if (ret != LIBUSB_SUCCESS)
				return;

			// claim interface with bInterfaceNumber = 0
			ret = libusb_claim_interface(handle, 0);
			if (ret != LIBUSB_SUCCESS)
				return;

			radio::handle = handle;
		}
	}
}

void init() {
	// radio connected via USB
	initUsb();
	
	// init emulated green power devices (on user interface)
	for (int i = 0; i < array::size(devices); ++i) {
		GreenPowerDeviceData const &d = deviceData[i];
		GreenPowerDevice &device = devices[i];
		device.channel = 11; // default channel
		device.deviceId = d.deviceId;
		device.counter = d.counter;
		setKey(device.key, d.key);
	}
}

void start(int channel) {
	assert(channel >= 10 && channel <= 26);
	radio::channel = channel;
}

void stop() {
	radio::channel = -1;
}

void detectEnergy(std::function<void (uint8_t)> const &onReady) {

}

void enableReceiver(bool enable) {

}

void setLongAddress(uint64_t longAddress) {
	radio::longAddressLo = uint32_t(longAddress);
	radio::longAddressHi = uint32_t(longAddress >> 32);
}

uint8_t allocate() {
	for (uint8_t index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
		Context &c = radio::contexts[index];
		if (c.flags == 0) {
			c.flags = 0x8000; // mark as allocated
			c.pan = 0xffff;
			c.shortAddress = 0xffff;
			return index + 1;
		}
	}
	
	// error: radio context array is full
	assert(false);
	return 0;
}

void setFlags(uint8_t id, uint16_t flags) {
	uint8_t index = id - 1;
	assert(index < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.flags = 0x8000 | flags;
}

void setPan(uint8_t id, uint16_t pan) {
	uint8_t index = id - 1;
	assert(index < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.pan = pan;
}

void setShortAddress(uint8_t id, uint16_t shortAddress) {
	uint8_t index = id - 1;
	assert(index < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.shortAddress = shortAddress;
}

void setReceiveHandler(uint8_t id, std::function<void (uint8_t *)> const &onReceived) {
	uint8_t index = id - 1;
	assert(index < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];
	
	c.onReceived = onReceived;
}

bool send(uint8_t const* data, std::function<void (bool)> const &onSent) {
	radio::onSent = onSent;

	return true;
}


} // namespace radio
