#include <usb.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <util.hpp>


enum class Request : uint8_t {
	RED = 0,
	GREEN = 1,
	BLUE = 2
};



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
	struct usb::EndpointDescriptor endpoints[2];
} __attribute__((packed));

static const UsbConfiguration configurationDescriptor = {
	.config = {
		.bLength = sizeof(usb::ConfigDescriptor),
		.bDescriptorType = usb::DescriptorType::CONFIGURATION,
		.wTotalLength = sizeof(UsbConfiguration),
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
		.bNumEndpoints = array::size(configurationDescriptor.endpoints),
		.bInterfaceClass = 0xff, // no class
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0
	},
	.endpoints = {{
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DescriptorType::ENDPOINT,
		.bEndpointAddress = 1 | usb::IN, // 1 in (device to host)
		.bmAttributes = usb::EndpointType::BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	},
	{
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DescriptorType::ENDPOINT,
		.bEndpointAddress = 1 | usb::OUT, // 1 out (host to device)
		.bmAttributes = usb::EndpointType::BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	}}
};


uint8_t sendData[128] __attribute__((aligned(4)));
uint8_t receiveData[128] __attribute__((aligned(4)));


void receive() {
	// wait until the host sends data
	usb::receive(1, receiveData, array::size(receiveData), [](int length) {
		//debug::toggleGreenLed();

		// check received data
		bool error = false;
		for (int i = 0; i < length; ++i) {
			if (receiveData[i] != length + i)
				error = true;
		}
		if (error)
			debug::setRedLed(true);

		// send received data back to host
		for (int i = 0; i < length; ++i) {
			sendData[i] = receiveData[i];
		}
		usb::send(1, sendData, length, []() {
			debug::toggleBlueLed();
		});
		
		// receive from usb host again
		receive();
	});
}

int main(void) {
	loop::init();
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
			// enable bulk endpoints 1 in and 1 out (keep control endpoint 0 enabled)
			usb::enableEndpoints(1 | (1 << 1), 1 | (1 << 1)); 			
		
			// start to receive from usb host
			receive();
		},
		[](uint8_t bRequest, uint16_t wValue, uint16_t wIndex) {
			switch (Request(bRequest)) {
			case Request::RED:
				debug::setRedLed(wValue != 0);
				break;
			case Request::GREEN:
				debug::setGreenLed(wIndex != 0);
				break;
			case Request::BLUE:
				debug::setBlueLed(wValue == wIndex);
				break;
			default:
				return false;
			}
			return true;
		});
	debug::init();
		
	loop::run();
}
