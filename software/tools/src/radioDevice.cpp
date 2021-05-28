#include <usb.hpp>
#include <rng.hpp>
#include <radio.hpp>
#include <loop.hpp>
#include <debug.hpp>
#include <util.hpp>
#include <sysConfig.hpp>
#include <Queue.hpp>


/*
	Radio device for emulator and sniffer
*/

// device descriptor
static const usb::DeviceDescriptor deviceDescriptor = {
	.bLength = sizeof(usb::DeviceDescriptor),
	.bDescriptorType = usb::DescriptorType::DEVICE,
	.bcdUSB = 0x0200, // USB 2.0
	.bDeviceClass = 0xff, // no class
	.bDeviceSubClass = 0xff,
	.bDeviceProtocol = 0, // 0 = binary, 1 = text
	.bMaxPacketSize0 = 64, // max packet size for endpoint 0
	.idVendor = 0x1915, // Nordic Semoconductor
	.idProduct = 0x1337,
	.bcdDevice = 0x0100, // device version
	.iManufacturer = 0, // index into string table
	.iProduct = 0, // index into string table
	.iSerialNumber = 0, // index into string table
	.bNumConfigurations = 1
};

// configuration descriptor
struct UsbConfiguration {
	struct usb::ConfigDescriptor config;
	struct usb::InterfaceDescriptor interface;
	struct usb::EndpointDescriptor endpoints[4 * 2];
} __attribute__((packed));

static const UsbConfiguration configurationDescriptor = {
	.config = {
		.bLength = sizeof(usb::ConfigDescriptor),
		.bDescriptorType = usb::DescriptorType::CONFIGURATION,
		.wTotalLength = offsetof(UsbConfiguration, endpoints) + sizeof(usb::EndpointDescriptor) * RADIO_CONTEXT_COUNT * 2,
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = 0x80, // bus powered
		.bMaxPower = 50 // 100 mA
	},
	.interface = {
		.bLength = sizeof(usb::InterfaceDescriptor),
		.bDescriptorType = usb::DescriptorType::INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = RADIO_CONTEXT_COUNT * 2,
		.bInterfaceClass = 0xff, // no class
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0
	},
	.endpoints = {
		{
			// endpoint 1 in (device to host, radio receive)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 1 | usb::IN,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		},
		{
			// endpoint 1 out (host to device, radio send)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 1 | usb::OUT,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		},
		{
			// endpoint 2 in (device to host, radio receive)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 2 | usb::IN,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		},
		{
			// endpoint 2 out (host to device, radio send)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 2 | usb::OUT,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		},
		{
			// endpoint 3 in (device to host, radio receive)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 3 | usb::IN,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		},
		{
			// endpoint 3 out (host to device, radio send)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 3 | usb::OUT,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		},
		{
			// endpoint 4 in (device to host, radio receive)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 4 | usb::IN,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		},
		{
			// endpoint 4 out (host to device, radio send)
			.bLength = sizeof(usb::EndpointDescriptor),
			.bDescriptorType = usb::DescriptorType::ENDPOINT,
			.bEndpointAddress = 4 | usb::OUT,
			.bmAttributes = usb::EndpointType::BULK,
			.wMaxPacketSize = 64,
			.bInterval = 1 // polling interval
		}		
	}
};

// check if number of radio context does not exceed the number of usb endpoints
static_assert(RADIO_CONTEXT_COUNT * 2 <= array::size(configurationDescriptor.endpoints));


struct Buffer {
	uint8_t data[1 + RADIO_MAX_PAYLOAD_LENGTH + 1];
} __attribute__((aligned(4)));

constexpr int RESULT_INDEX = 1 + RADIO_MAX_PAYLOAD_LENGTH;

struct Context {
	// queue of packets received by the radio
	Queue<Buffer, 64> receiveQueue;
	
	// queue for packets to be sent by the radio
	Queue<Buffer, 64> sendQueue;

	// check if usb input is available
	bool hasUsbIn() {
		return !receiveQueue.empty()
			|| (!sendQueue.empty() && sendQueue.get().data[RESULT_INDEX] != radio::Result::WAITING);
	}
};

Context contexts[RADIO_CONTEXT_COUNT];



void usbIn(int index);
void usbOut(int index);


// send to usb host
void usbIn(int index) {
	Context &context = contexts[index];

	// check if the result of a radio send is available
	if (!context.sendQueue.empty()) {
		Buffer &buffer = context.sendQueue.get();
		if (buffer.data[RESULT_INDEX] != 0) {
			// send to usb host
			usb::send(1 + index, buffer.data + RESULT_INDEX, 1, [index]() {
				Context &context = contexts[index];
				bool full = context.sendQueue.full();
				context.sendQueue.remove();
debug::setRedLed(!context.sendQueue.empty());
				
				// receive from usb host again when queue was full
				if (full)
					usbOut(index);				
			
				// continue to send to usb host if more available
				if (context.hasUsbIn())
					usbIn(index);
			});
			return;
		}
	}

	Buffer &buffer = context.receiveQueue.get();
	
	// get length from first byte
	int length = buffer.data[0];
	
	// send to usb host
	usb::send(1 + index, buffer.data + 1, length, [index]() {
		Context &context = contexts[index];
		debug::toggleBlueLed();
		
		context.receiveQueue.remove();
		
		// continue to send to usb host if more available
		if (context.hasUsbIn())
			usbIn(index);
	});	
}

// send over the air
void send(int index) {
	Context &context = contexts[index];
	Buffer &buffer = context.sendQueue.get();

	radio::send(index, buffer.data, [index](bool success) {
		Context &context = contexts[index];
		bool hasUsbIn = context.hasUsbIn();
		
		// set result of send operation
		Buffer &buffer = context.sendQueue.get();
		buffer.data[RESULT_INDEX] = success ? radio::Result::SUCCESS : radio::Result::FAILURE;
		
		// start to send to usb host if necessary to transfer the result
		if (!hasUsbIn)
			usbIn(index);
	});
}

// receive from usb host
void usbOut(int index) {
	Context &context = contexts[index];

	Buffer &buffer = context.sendQueue.getHead();

	// wait until the host sends data
	usb::receive(1 + index, buffer.data + 1, RADIO_MAX_PAYLOAD_LENGTH, [index](int length) {
		Context &context = contexts[index];
		
		bool empty = context.sendQueue.empty();
		
		//debug::toggleGreenLed();
		// allocate queue element (data is already in the buffer)
		Buffer &buffer = context.sendQueue.add();
debug::setRedLed();

		// set length to first byte with space for crc
		buffer.data[0] = length + 2;
		
		// set send result to waiting state 
		buffer.data[RESULT_INDEX] = radio::Result::WAITING;

		// start sending over the air if queue was empty
		if (empty)
			send(index);

		// receive from usb host again when queue is not full
		if (!context.sendQueue.full())
			usbOut(index);
	});
}

int main(void) {
	loop::init();
	rng::init(); // needed by radio
	radio::init();
	usb::init(
		[](usb::DescriptorType descriptorType) {
			switch (descriptorType) {
			case usb::DescriptorType::DEVICE:
				return Data(&deviceDescriptor);
			case usb::DescriptorType::CONFIGURATION:
				return Data(&configurationDescriptor);
			default:
				return Data();
			}
		},
		[](uint8_t bConfigurationValue) {
			// enable bulk endpoints (keep control endpoint 0 enabled in both directions)
			int flags = ~(0xffffffff << (1 + RADIO_CONTEXT_COUNT));
			usb::enableEndpoints(flags, flags); 			

			// start to receive from usb host
			for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
				usbOut(index);
			}
		},
		[](uint8_t bRequest, uint16_t wValue, uint16_t wIndex) {
			switch (radio::Request(bRequest)) {
			case radio::Request::RESET:
				radio::stop();
				for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
					radio::setFlags(index, 0);
					radio::setPan(index, 0xffff);
					radio::setShortAddress(index, 0xffff);
				}
				break;
			case radio::Request::START:
				// check if the channel is in the allowed range
				if (wValue >= 10 && wValue <= 26)
					radio::start(wValue);
				break;
			case radio::Request::STOP:
				radio::stop();
				break;
			case radio::Request::ENABLE_RECEIVER:
				radio::enableReceiver(wValue != 0);
				break;
			case radio::Request::SET_FLAGS:
				if (wIndex < RADIO_CONTEXT_COUNT)
					radio::setFlags(wIndex, wValue);
				break;
			case radio::Request::SET_PAN:
				if (wIndex < RADIO_CONTEXT_COUNT)
					radio::setPan(wIndex, wValue);
				break;
			case radio::Request::SET_SHORT_ADDRESS:
				if (wIndex < RADIO_CONTEXT_COUNT)
					radio::setShortAddress(wIndex, wValue);
				break;
			default:
				return false;
			}
			return true;
		});
	debug::init();
	
	// set radio receive handlers
	for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
		radio::setReceiveHandler(index, [index](uint8_t *data) {
			Context &context = contexts[index];
			if (!context.receiveQueue.full()) {
				bool hasUsbIn = context.hasUsbIn();
				Buffer &buffer = context.receiveQueue.add();
				
				// length without crc but with one byte for LQI
				int length = data[0] - 2 + 1;
				
				// check if packet has minimum length (2 bytes frame control, one byte LQI)
				if (length >= 2 + 1) {
					buffer.data[0] = length;
					
					// copy data
					array::copy(buffer.data + 1, buffer.data + 1 + length, data + 1);
				
					// start to send to usb host if necessary
					if (!hasUsbIn)
						usbIn(index);
				}
			}
		});
	}

	// start radio, enable baseband decoder and receive all packets for sniffer or terminal
	radio::start(15);
	radio::enableReceiver(true);
	radio::setFlags(0, radio::ALL);

	loop::run();
}
