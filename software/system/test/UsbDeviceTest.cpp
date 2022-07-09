#include <UsbDevice.hpp>
#include <Debug.hpp>
#include <Loop.hpp>
#include <Coroutine.hpp>
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
		.bNumEndpoints = array::count(configurationDescriptor.endpoints),
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



uint8_t buffer[128] __attribute__((aligned(4)));

Coroutine echo() {
	while (true) {
		// receive data from host
		int length = array::count(buffer);
		co_await UsbDevice::receive(1, length, buffer);

		// check received data
		bool error = false;
		for (int i = 0; i < length; ++i) {
			if (buffer[i] != length + i)
				error = true;
		}
		if (error)
			Debug::setRedLed(true);
		
		// send data back to host
		co_await UsbDevice::send(1, length, buffer);

		Debug::toggleBlueLed();
	}
}

int main(void) {
	Loop::init();
	UsbDevice::init(
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
		[](uint8_t bConfigurationValue) {
			// enable bulk endpoints 1 in and 1 out (keep control endpoint 0 enabled)
			UsbDevice::enableEndpoints(1 | (1 << 1), 1 | (1 << 1));
		
			// start to receive from usb host
			echo();
		},
		[](uint8_t bRequest, uint16_t wValue, uint16_t wIndex) {
			switch (Request(bRequest)) {
			case Request::RED:
				Debug::setRedLed(wValue != 0);
				break;
			case Request::GREEN:
				Debug::setGreenLed(wIndex != 0);
				break;
			case Request::BLUE:
				Debug::setBlueLed(wValue == wIndex);
				break;
			default:
				return false;
			}
			return true;
		});
	Output::init(); // for debug led's
		
	Loop::run();
}
