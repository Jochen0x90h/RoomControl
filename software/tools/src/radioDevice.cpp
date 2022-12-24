#include <UsbDevice.hpp>
#include <Random.hpp>
#include <Radio.hpp>
#include <Timer.hpp>
#include <Loop.hpp>
#include <Debug.hpp>
#include <util.hpp>
#include <Queue.hpp>
#include <boardConfig.hpp>


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
	.idVendor = 0x1915, // Nordic Semiconductor
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
static_assert(RADIO_CONTEXT_COUNT * 2 <= array::count(configurationDescriptor.endpoints));

// for each context and mac counter a barrier to be able to cancel send requests
Barrier<> barriers[RADIO_CONTEXT_COUNT][256];

// receive from radio and send to usb host
Coroutine receive(UsbDevice &usb, int index) {
	Radio::Packet packet;
	while (true) {
		// receive from radio
		co_await Radio::receive(index, packet);
		debug::setGreen(true);

		// length without crc but with extra data
		int length = packet[0] - 2 + Radio::RECEIVE_EXTRA_LENGTH;
		
		// check if packet has minimum length (2 bytes frame control and extra data)
		if (length >= 2 + Radio::RECEIVE_EXTRA_LENGTH) {			
			// send to usb host
			co_await usb.send(1 + index, length, packet + 1); // IN
		}
		debug::setGreen(false);
	}
}


// receive from usb host and send to radio
Coroutine send(UsbDevice &usb, int index) {
	Radio::Packet packet;
	while (true) {
		// receive from usb host
		int length = RADIO_MAX_PAYLOAD_LENGTH + Radio::SEND_EXTRA_LENGTH;
		co_await usb.receive(1 + index, length, packet + 1); // OUT

		if (length == 1) {
			// cancel by mac counter
			uint8_t macCounter = packet[1];
			barriers[index][macCounter].resumeAll();
		} else if (length >= 2 + Radio::SEND_EXTRA_LENGTH) {
			debug::setRed(true);

			// set length to first byte (subtract extra data but add space for crc)
			packet[0] = length - Radio::SEND_EXTRA_LENGTH + 2;

			// get mac counter to identify the packet
			uint8_t macCounter = packet[3];

			// send over the air
			uint8_t result = 1;
			int r = co_await select(Radio::send(index, packet, result), barriers[index][macCounter].wait());

			if (r == 1) {
				// send mac counter and result back to usb host
				packet[0] = macCounter;
				packet[1] = result;
				co_await usb.send(1 + index, 2, packet); // IN
			}
			debug::setRed(false);
		}
	}
}


int main(void) {
	Loop::init();
	Timer::init();
	Radio::init();
	UsbDeviceImpl usb(
		[](usb::DescriptorType descriptorType) {
			switch (descriptorType) {
			case usb::DescriptorType::DEVICE:
				return ConstData(&deviceDescriptor);
			case usb::DescriptorType::CONFIGURATION:
				return ConstData(&configurationDescriptor);
			default:
				return ConstData();
			}
		},
		[](UsbDevice &usb, uint8_t bConfigurationValue) {
			// enable bulk endpoints (keep control endpoint 0 enabled in both directions)
			int flags = ~(0xffffffff << (1 + RADIO_CONTEXT_COUNT));
			usb.enableEndpoints(flags, flags);
		},
		[](uint8_t bRequest, uint16_t wValue, uint16_t wIndex) {
			switch (Radio::Request(bRequest)) {
			case Radio::Request::RESET:
				Radio::stop();
				for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
					Radio::setFlags(index, Radio::ContextFlags::NONE);
					Radio::setPan(index, 0xffff);
					Radio::setShortAddress(index, 0xffff);
				}
				break;
			case Radio::Request::START:
				// check if the channel is in the allowed range
				if (wValue >= 10 && wValue <= 26)
					Radio::start(wValue);
				break;
			case Radio::Request::STOP:
				Radio::stop();
				break;
			case Radio::Request::ENABLE_RECEIVER:
				Radio::enableReceiver(wValue != 0);
				break;
			case Radio::Request::SET_LONG_ADDRESS:
				// todo
				break;
			case Radio::Request::SET_PAN:
				if (wIndex < RADIO_CONTEXT_COUNT)
					Radio::setPan(wIndex, wValue);
				break;
			case Radio::Request::SET_SHORT_ADDRESS:
				if (wIndex < RADIO_CONTEXT_COUNT)
					Radio::setShortAddress(wIndex, wValue);
				break;
			case Radio::Request::SET_FLAGS:
				if (wIndex < RADIO_CONTEXT_COUNT)
					Radio::setFlags(wIndex, Radio::ContextFlags(wValue));
				break;
			default:
				return false;
			}
			return true;
		});
	Output::init();

	// start radio, enable baseband decoder and pass all packets for sniffer or terminal
	Radio::start(15);
	Radio::enableReceiver(true);
	Radio::setFlags(0, Radio::ContextFlags::PASS_ALL);

	// start coroutines to send and receive
	for (int index = 0; index < RADIO_CONTEXT_COUNT; ++index) {
		for (int i = 0; i < 64; ++i) {
			receive(usb, index);
			send(usb, index);
		}
	}

	Loop::run();
}
