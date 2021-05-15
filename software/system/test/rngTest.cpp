#include <timer.hpp>
#include <rng.hpp>
#include <usb.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <util.hpp>


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
	struct usb::EndpointDescriptor endpoints[1];
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
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = usb::IN | 1, // in 1 (tx)
		.bmAttributes = usb::ENDPOINT_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	}}
};



uint8_t timer1;
uint8_t sendData[16] __attribute__((aligned(4)));

void setTimer1() {
	for (int i = 0; i < array::size(sendData); ++i)
		sendData[i] = rng::get8();
		
	usb::send(1, sendData, array::size(sendData), []() {debug::toggleBlueLed();});
	timer::start(timer1, timer::getTime() + 1s, []() {setTimer1();});
}

int main(void) {
	loop::init();
	timer::init();
	rng::init();
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
			// enable bulk endpoints in 1 (keep control endpoint 0 enabled)
			usb::enableEndpoints(1 | (1 << 1), 1); 			

			setTimer1();
		});
	debug::init();

	timer1 = timer::allocate();
		
	loop::run();
}
