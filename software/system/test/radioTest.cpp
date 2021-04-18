#include <usb.hpp>
#include <radio.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <util.hpp>
#include <config.hpp>


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
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = usb::IN | 1, // in 1 (tx)
		.bmAttributes = usb::ENDPOINT_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	},
	{
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = usb::OUT | 2, // out 2 (rx)
		.bmAttributes = usb::ENDPOINT_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	}}
};

uint8_t buffer[RADIO_MAX_PAYLOAD_LENGTH] __attribute__((aligned(4)));

int main(void) {
	loop::init();
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
		});
	debug::init();
		
	//radio::setChannel(15);

	// add a receive handler to the radio
	radio::addReceiveHandler([](uint8_t *data, int length) {		
		array::copy(buffer, buffer + length, data);
		usb::send(1, buffer, length, []() {
			debug::toggleBlueLed();
		});	
	});
	
	loop::run();
}
