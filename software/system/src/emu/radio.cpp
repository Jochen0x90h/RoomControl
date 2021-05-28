#include <radio.hpp>
#include <usbDefs.hpp>
#include <emu/global.hpp>
#include <emu/loop.hpp>
#include <util.hpp>
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


// receiver
// --------

// let the emulated radio receive some data
void receiveData(int channel, uint8_t *data) {
	if (channel == radio::channel) {
		for (auto const &c : radio::contexts) {
			if (c.onReceived && c.filter(data))
				c.onReceived(data);
		}
	}
}


// sender
// ------

// send queue
struct Send {
	enum State : uint8_t {
		// send and don't wait for ack
		AWAIT_SENT,
		
		// onSent is currently being called
		CALL
	};

	std::function<void (bool)> onSent;
	State state;
};
Queue<Send, RADIO_SEND_QUEUE_LENGTH> sendQueue;


// radio connected via USB
// -----------------------

libusb_device_handle *handle = nullptr;
asio::steady_timer *timer = nullptr;

// poll usb device
void poll() {
	if (radio::handle != nullptr) {
		// receive buffer: length, payload, LQI
		uint8_t data[1 + RADIO_MAX_PAYLOAD_LENGTH + 1];
		
		for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
			// receive from radio
			while (true) {
				int length;
				int ret = libusb_bulk_transfer(radio::handle, 1 + index | usb::IN, data + 1, array::size(data) - 1, &length, 1);
				if (ret != LIBUSB_SUCCESS)
					break;
				if (length >= 2) {
					// convert to radio format where first byte is length of payload including crc
					data[0] = length + 2;

					// call receive handler
					auto const &c = radio::contexts[index];
					if (c.onReceived)
						c.onReceived(data);
				} else if (length == 1) {
					if (data[1] <= Result::MAX_ED_VALUE) {
						// got a result of an energy detection
					} else if (!sendQueue.empty()) {
						// got a result of a send operation
						Send &send = sendQueue.get();

						// set to call state so that stop() doesn't remove the queue entry
						send.state = Send::CALL;
						if (send.onSent)
							send.onSent(data[1] == Result::SUCCESS);
						
						sendQueue.remove();
					}
				}
			}
		}
	}
		
	// call again
	radio::timer->expires_at(std::chrono::steady_clock::now() + std::chrono::milliseconds(50));
	radio::timer->async_wait([] (boost::system::error_code error) {
		if (!error) {
			poll();
		}
	});
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
			libusb_device_handle *handle;
			ret = libusb_open(dev, &handle);
			if (ret != LIBUSB_SUCCESS)
				continue;
			
			// set configuration (reset alt_setting, reset toggles)
			ret = libusb_set_configuration(handle, 1);
			if (ret != LIBUSB_SUCCESS)
				continue;

			// claim interface with bInterfaceNumber = 0
			ret = libusb_claim_interface(handle, 0);
			if (ret != LIBUSB_SUCCESS)
				continue;

			// reset the radio
			controlTransfer(handle, Request::RESET, 0, 0);

			radio::handle = handle;
			
			radio::timer = new asio::steady_timer(loop::context);
			poll();
		}
	}
}

void start(int channel) {
	assert(channel >= 10 && channel <= 26);
	radio::channel = channel;

	if (radio::handle != nullptr)
		controlTransfer(radio::handle, Request::START, channel, 0);
}

void stop() {
	radio::channel = -1;

	if (radio::handle != nullptr)
		controlTransfer(radio::handle, Request::STOP, 0, 0);

	// clear queues
	if (!radio::sendQueue.empty()) {
		Send &send = sendQueue.get();
		radio::sendQueue.clear();
		
		// preserve element if onSent is currently called
		if (send.state == Send::CALL)
			radio::sendQueue.increment();
	}
}

void detectEnergy(std::function<void (uint8_t)> const &onReady) {

}

void enableReceiver(bool enable) {
	if (radio::handle != nullptr)
		controlTransfer(radio::handle, Request::ENABLE_RECEIVER, uint16_t(enable), 0);
}

void setLongAddress(uint64_t longAddress) {
	radio::longAddressLo = uint32_t(longAddress);
	radio::longAddressHi = uint32_t(longAddress >> 32);
}

void setFlags(int index, uint16_t flags) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.flags = flags;

	if (radio::handle != nullptr)
		controlTransfer(radio::handle, Request::SET_FLAGS, flags, index);
}

void setPan(int index, uint16_t pan) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.pan = pan;

	if (radio::handle != nullptr)
		controlTransfer(radio::handle, Request::SET_FLAGS, pan, index);
}

void setShortAddress(int index, uint16_t shortAddress) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];

	c.shortAddress = shortAddress;

	if (radio::handle != nullptr)
		controlTransfer(radio::handle, Request::SET_FLAGS, shortAddress, index);
}

void setReceiveHandler(int index, std::function<void (uint8_t *)> const &onReceived) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);
	Context &c = radio::contexts[index];
	
	c.onReceived = onReceived;
}

bool send(int index, uint8_t const* data, std::function<void (bool)> const &onSent) {
	assert(uint(index) < RADIO_CONTEXT_COUNT);

	// check if send queue is full
	if (radio::sendQueue.full())
		return false;

	// get length without crc
	int length = data[0] - 2;
	
	if (radio::handle != nullptr) {
		int transferred;
		uint8_t *d = const_cast<uint8_t *>(data + 1);
		
		int ret = libusb_bulk_transfer(handle, 1 + index | usb::OUT, d, length, &transferred, 10000);
		if (ret != 0) {
			loop::context.post([onSent]() {onSent(false);});
			return true;
		}
		
		// send zero length packet to indicate that transfer is complete if length is multiple of 64
		if (length > 0 && (length & 63) == 0)
			libusb_bulk_transfer(handle, (1 + index) | usb::OUT, d, 0, &transferred, 1000);

		Send &send = radio::sendQueue.add();
		
		// set callback and state
		send.onSent = onSent;
		send.state = Send::AWAIT_SENT;
	} else {
		loop::context.post([onSent]() {onSent(false);});
	}

	return true;
}

} // namespace radio
