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


// receive from radio and send to usb host
Coroutine receive(int index) {
	while (true) {
		radio::Packet packet;
		
		// receive from radio
		co_await radio::receive(index, packet);
		debug::setGreenLed(true);

		// length without crc but with one byte for LQI
		int length = packet[0] - 2 + 1;
		
		// check if packet has minimum length (2 bytes frame control, one byte LQI)
		if (length >= 2 + 1) {			
			// send to usb host
			co_await usb::send(1 + index, length, packet + 1);
		}
		debug::setGreenLed(false);
	}
}

// receive from usb host and send to radio
Coroutine send(int index) {
	while (true) {
		radio::Packet packet;

		// receive from usb host
		int length;
		co_await usb::receive(1 + index, RADIO_MAX_PAYLOAD_LENGTH, length, packet + 1);
		debug::setRedLed(true);

		// set length to first byte with space for crc
		packet[0] = length + 2;
		
		// send over the air
		uint8_t result;
		co_await::radio::send(index, packet, result);
	
		// send result back to usb host
		result += radio::Result::MIN_RESULT_VALUE;
		co_await usb::send(1 + index, 1, &result);
		debug::setRedLed(false);
	}
}


int main(void) {
	loop::init();
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

			// start to send and receive
			for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
				receive(index);
				send(index);
			}
		},
		[](uint8_t bRequest, uint16_t wValue, uint16_t wIndex) {
			switch (radio::Request(bRequest)) {
			case radio::Request::RESET:
				radio::stop();
				for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
					radio::setFlags(index, radio::ContextFlags::NONE);
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
					radio::setFlags(wIndex, radio::ContextFlags(wValue));
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

	// start radio, enable baseband decoder and pass all packets for sniffer or terminal
	radio::start(15);
	radio::enableReceiver(true);
	radio::setFlags(0, radio::ContextFlags::PASS_ALL);

	loop::run();
}
