#include <usb.hpp>
#include <rng.hpp>
#include <radio.hpp>
#include <loop.hpp>
#include <debug.hpp>
#include <util.hpp>
#include <config.hpp>
#include <Queue.hpp>


/*
	Radio device for emulator and sniffer
*/

// device descriptor
static const usb::DeviceDescriptor deviceDescriptor = {
	.bLength = sizeof(usb::DeviceDescriptor),
	.bDescriptorType = usb::DESCRIPTOR_DEVICE,
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
	struct usb::EndpointDescriptor endpoints[2];
} __attribute__((packed));

static const UsbConfiguration configurationDescriptor = {
	.config = {
		.bLength = sizeof(usb::ConfigDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_CONFIGURATION,
		.wTotalLength = sizeof(UsbConfiguration),
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = 0x80, // bus powered
		.bMaxPower = 50 // 100 mA
	},
	.interface = {
		.bLength = sizeof(usb::InterfaceDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = array::size(configurationDescriptor.endpoints),
		.bInterfaceClass = 0xff, // no class
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0
	},
	.endpoints = {{
		// endpoint 1 in (device to host, radio receive)
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = 1 | usb::IN,
		.bmAttributes = usb::ENDPOINT_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	},
	{
		// endpoint 2 out (host to device, radio send)
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = 2 | usb::OUT,
		.bmAttributes = usb::ENDPOINT_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	}}
};

uint8_t radioId;

using Buffer = uint8_t[(1 + RADIO_MAX_PAYLOAD_LENGTH + 1 + 3) & ~3];

// send to usb host
Queue<Buffer, 4> sendQueue __attribute__((aligned(4)));

void send() {
	Buffer &buffer = sendQueue.get();
	
	// get length from first byte
	int length = buffer[0];
	
	// send to usb host
	usb::send(1, buffer + 1, length, []() {
		debug::toggleBlueLed();
		
		sendQueue.remove();
		
		// continue to send if queue not empty yet
		if (!sendQueue.empty())
			send();
	});	
}

// receive from usb host
Queue<Buffer, 4> receiveQueue __attribute__((aligned(4)));

void receive() {
	// wait until the host sends data
	Buffer &buffer = receiveQueue.getHead();
	usb::receive(2, buffer + 1, RADIO_MAX_PAYLOAD_LENGTH, [](int length) {
		//debug::toggleGreenLed();
		Buffer &buffer = receiveQueue.getHead();

		// set length to first byte
		buffer[0] = length;
		receiveQueue.increment();
		
		// send over the air
		radio::send(radioId, buffer, [](bool) {
			bool full = receiveQueue.full();
			receiveQueue.remove();
			
			// receive from usb host again when queue was full
			if (full)
				receive();
		});
		
		// receive from usb host again when queue is not full
		if (!receiveQueue.full())
			receive();
	});
}


int main(void) {
	loop::init();
	rng::init(); // needed by radio
	radio::init();
	usb::init(
		[](usb::DescriptorType descriptorType) {
			switch (descriptorType) {
			case usb::DESCRIPTOR_DEVICE:
				return Data(deviceDescriptor);
			case usb::DESCRIPTOR_CONFIGURATION:
				return Data(configurationDescriptor);
			default:
				return Data();
			}
		},
		[](uint8_t bConfigurationValue) {
			// enable bulk endpoints in 1 and out 2 (keep control endpoint 0 enabled in both directions)
			usb::enableEndpoints(1 | (1 << 1), 1 | (1 << 2)); 			
		
			// start to receive from usb host
			receive();
		});
	debug::init();
		
	// allocate a radio context
	radioId = radio::allocate();
	
	radio::setFlags(radioId, radio::ALL);	

radio::setLongAddress(UINT64_C(0x00124b00214f3c55));
radio::setPan(radioId, 0x1a62);
radio::setShortAddress(radioId, 0x0000);
radio::setFlags(radioId, radio::TYPE_BEACON | radio::DEST_SHORT | radio::HANDLE_ACK);
	
	// set radio receive handler
	radio::setReceiveHandler(radioId, [](uint8_t *data) {		
		if (!sendQueue.full()) {
			bool empty = sendQueue.empty();
			Buffer &buffer = sendQueue.add();
			
			// copy data including length byte
			int length = max(data[0] - 2, 0) + 1; // for LQI
			buffer[0] = length;
			array::copy(buffer + 1, buffer + 1 + length, data + 1);
		
			// start to send to usb host if queue was empty
			if (empty)
				send();
		}
	});

	// start radio and enable baseband decoder
	radio::start(15);
	radio::enableReceiver(true);
	
	loop::run();
}
