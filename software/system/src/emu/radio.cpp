#include <radio.hpp>
#include <usbDefs.hpp>
#include <emu/global.hpp>
#include <emu/loop.hpp>
#include <crypt.hpp>
#include <ieee.hpp>
#include <Nonce.hpp>
#include <Queue.hpp>
#include <sysConfig.hpp>
#include <libusb.h>


namespace radio {

// the channel the radio receives and sends on
int channel;


// context
// -------

uint32_t longAddressLo;
uint32_t longAddressHi;

struct Context {
	ContextFlags volatile flags;
	uint16_t volatile pan;
	uint16_t volatile shortAddress;

	CoList<ReceiveParameters> receiveWaitingList;
	CoList<SendParameters> sendWaitingList;

	
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
					if (longAddressLo == radio::longAddressLo && longAddressHi == radio::longAddressHi)
						return true;
				}
			}
		}
		
		return false;
	}
};

Context contexts[RADIO_CONTEXT_COUNT];


// let the emulated radio receive some data
void receiveData(int channel, uint8_t *packet) {
	if (channel == radio::channel) {
		for (auto &context : radio::contexts) {
			if (context.filter(packet)) {
				// resume first waiting coroutine
				context.receiveWaitingList.resumeFirst([packet](ReceiveParameters p) {
					// length without crc but with one byte for LQI
					int length = packet[0] - 2 + 1;
					array::copy(1 + length, p.packet, packet);
					return true;
				});
			}
		}
	}
}


// radio connected via USB
// -----------------------

libusb_device_handle *device = nullptr;


// emulated radio devices
// ----------------------

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
	{0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf}
}};


// event loop handler chain
loop::Handler nextHandler;
void handle(Gui &gui) {
	for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
		auto &context = radio::contexts[index];
	
		// resume all failed send operations (send operations fail e.g. when there is no usb radio device)
		context.sendWaitingList.resumeAll([](SendParameters p) {
			if (p.packet == nullptr) {
				p.result = 0;
				return true;
			}
			return false;
		});
	
		while (true) {
			if (radio::device != nullptr) {
				// receive buffer: length, payload, LQI
				radio::Packet packet;

				int length;
				int ret = libusb_bulk_transfer(radio::device, 1 + index | usb::IN, packet + 1, array::size(packet) - 1, &length, 1);
				if (ret != LIBUSB_SUCCESS)
					break;
				if (length >= 2) {
					// resume first waiting coroutine
					uint8_t *d = packet;
					context.receiveWaitingList.resumeFirst([length, d](ReceiveParameters p) {
						// convert to radio format where first byte is length of payload and crc, but no LQI
						p.packet[0] = length - 1 + 2;
						array::copy(length, p.packet + 1, d + 1);
						return true;
					});
				} else if (length == 1) {
					// length of 1 indicates a result for detectEnergy() and send()
					if (packet[1] <= Result::MAX_ED_VALUE) {
						// got a result of an energy detection
					
					} else {
						// got a result of a send operation
						uint8_t result = packet[1] - Result::MIN_RESULT_VALUE;

						// resume first waiting coroutine
						context.sendWaitingList.resumeFirst([result](SendParameters p) {
							p.result = result;
							return true;
						});
					}
				}
			} else {
				break;
			}
		}
	}
	
	// call next handler in chain
	radio::nextHandler(gui);

	// emulated devices on user interface
	gui.newLine();
	int id = 0x308419a4;
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

				uint8_t data[] = {0,
					0x01, 0x08, uint8_t(device.counter), 0xff, 0xff, 0xff, 0xff, // mac header
					0x0c, // network header
					LE_INT32(device.deviceId), // deviceId
					0xe0, 0x02, 0xc5, 0xf2, // command and flags
					0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, // key
					LE_INT32(0), // mic
					LE_INT32(device.counter), // counter
					50}; // LQI
				data[0] = array::size(data) - 2 + 2;

				// nonce
				Nonce nonce(device.deviceId, device.deviceId);

				// header is deviceId
				uint8_t const *header = data + 9;
				int headerLength = 4;
				
				// payload is key
				uint8_t *message = data + 17;
				
				encrypt(message, nonce, header, headerLength, message, 16, 4, defaultAesKey);

				radio::receiveData(device.channel, data);
			} else {
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
					0x00, 0x00, 0x00, 0x00, // mic
					50}; // LQI
				data[0] = array::size(data) - 2 + 2;
				
				// nonce
				Nonce nonce(device.deviceId, device.counter);

				// header is network header, deviceId, counter and command
				uint8_t const *header = data + 8;
				int headerLength = 11;

				// message: empty payload and mic
				uint8_t *message = data + 19;

				encrypt(message, nonce, header, headerLength, message, 0, 4, device.key);

				radio::receiveData(device.channel, data);
			}
			++device.counter;
			device.lastRocker = rocker;
		}
	}
}

int controlTransfer(libusb_device_handle *handle, Request request, uint16_t wValue, uint16_t wIndex) {
	return libusb_control_transfer(handle,
		usb::OUT | usb::REQUEST_TYPE_VENDOR | usb::RECIPIENT_INTERFACE,
		uint8_t(request), wValue, wIndex,
		nullptr, 0, 1000);
}

void init() {
	// radio connected via USB
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

			radio::device = device;
		}
	}
	
	
	// initialize emulated green power devices (on user interface)
	for (int i = 0; i < array::size(devices); ++i) {
		GreenPowerDeviceData const &d = deviceData[i];
		GreenPowerDevice &device = devices[i];
		device.channel = 11; // default channel
		device.deviceId = d.deviceId;
		device.counter = d.counter;
		setKey(device.key, d.key);
	}


	// add to event loop handler chain
	radio::nextHandler = loop::addHandler(handle);
}

void start(int channel) {
	assert(channel >= 10 && channel <= 26);
	radio::channel = channel;

	if (radio::device != nullptr)
		controlTransfer(radio::device, Request::START, channel, 0);
}

void stop() {
	radio::channel = -1;

	if (radio::device != nullptr)
		controlTransfer(radio::device, Request::STOP, 0, 0);

	// clear queues
	for (auto &context : radio::contexts) {
		/*context.receiveWaitingQueue.resumeAll([](ReceiveParameters p) {
			// coroutines waiting for receive get a null pointer
			p.data = nullptr;
			return true;
		});*/
		context.sendWaitingList.resumeAll([](SendParameters p) {
			// coroutines waiting for send get a failure result
			p.result = 0;
			return true;
		});
	}
}

void detectEnergy(std::function<void (uint8_t)> const &onReady) {

}

void enableReceiver(bool enable) {
	if (radio::device != nullptr)
		controlTransfer(radio::device, Request::ENABLE_RECEIVER, uint16_t(enable), 0);
}

void setLongAddress(uint64_t longAddress) {
	radio::longAddressLo = uint32_t(longAddress);
	radio::longAddressHi = uint32_t(longAddress >> 32);
}

void setFlags(int index, ContextFlags flags) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.flags = flags;

	if (radio::device != nullptr)
		controlTransfer(radio::device, Request::SET_FLAGS, uint16_t(flags), index);
}

void setPan(int index, uint16_t pan) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.pan = pan;

	if (radio::device != nullptr)
		controlTransfer(radio::device, Request::SET_PAN, pan, index);
}

void setShortAddress(int index, uint16_t shortAddress) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.shortAddress = shortAddress;

	if (radio::device != nullptr)
		controlTransfer(radio::device, Request::SET_SHORT_ADDRESS, shortAddress, index);
}

Awaitable<ReceiveParameters> receive(int index, Packet &packet) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	auto &context = radio::contexts[index];

	return {context.receiveWaitingList, {packet}};
}

Awaitable<SendParameters> send(int index, uint8_t const *packet, uint8_t &result) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	auto &context = radio::contexts[index];

	// get length without crc
	int length = packet[0] - 2;

	// send to usb device if available
	if (radio::device != nullptr) {
		int transferred;
		uint8_t *d = const_cast<uint8_t *>(packet + 1);
		
		int ret = libusb_bulk_transfer(radio::device, 1 + index | usb::OUT, d, length, &transferred, 10000);
		if (ret == 0) {
			// send zero length packet to indicate that transfer is complete if length is multiple of 64
			if (length > 0 && (length & 63) == 0)
				libusb_bulk_transfer(radio::device, (1 + index) | usb::OUT, d, 0, &transferred, 1000);

		} else {
			// indicate failure
			packet = nullptr;
		}
	} else {
		// indicate failure
		packet = nullptr;
	}
	
	return {context.sendWaitingList, {packet, result}};
}

} // namespace radio
