#include <timer.hpp>
#include <usb.hpp>
#include <radio.hpp>
#include <debug.hpp>
#include <util.hpp>


// device descriptor
static const usb::DeviceDescriptor deviceDescriptor = {
	.bLength = sizeof(usb::DeviceDescriptor),
	.bDescriptorType = usb::DESCRIPTOR_DEVICE,
	.bcdUSB = 0x0200, // USB 2.0
	.bDeviceClass = 0xff, // no class
	.bDeviceSubClass = 0xff,
	.bDeviceProtocol = 0xff,
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
	struct usb::EndpointDescriptor endpoint1;
	struct usb::EndpointDescriptor endpoint2;
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
		.bNumEndpoints = 2,
		.bInterfaceClass = 0xff, // no class
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0
	},
	.endpoint1 = {
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = usb::IN | 1, // in 1 (tx)
		.bmAttributes = usb::ENDPOINT_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	},
	.endpoint2 = {
		.bLength = sizeof(usb::EndpointDescriptor),
		.bDescriptorType = usb::DESCRIPTOR_ENDPOINT,
		.bEndpointAddress = usb::OUT | 2, // out 2 (rx)
		.bmAttributes = usb::ENDPOINT_BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	}
};



void receive() {
	radio::setReceiveHandler([](uint8_t const *data) {
		int length = 1 + data[0] - 2; // crc is not in data
		
		usb::send(1, data, length, []() {
			debug::toggleBlueLed();
		});	
		
		// wait until we receive the next packet
		receive();
	});
}

int main(void) {
	debug::init();
	timer::init();
	radio::init();
	usb::init(
		[](usb::DescriptorType descriptorType) {
			switch (descriptorType) {
			case usb::DESCRIPTOR_DEVICE:
				return Array<uint8_t>(reinterpret_cast<uint8_t const *>(&deviceDescriptor), sizeof(deviceDescriptor));
			case usb::DESCRIPTOR_CONFIGURATION:
				return Array<uint8_t>(reinterpret_cast<uint8_t const *>(&configurationDescriptor), sizeof(configurationDescriptor));
			default:
				return Array<uint8_t>();
			}
		},
		[](uint8_t bConfigurationValue) {
			// enable bulk endpoints in 1 and out 2 (keep control endpoint 0 enabled)
			usb::enableEndpoints(1 | (1 << 1), 1 | (1 << 2)); 			
		
			// wait until we receive a packet
			receive();
		});
		
	//radio::setChannel(15);
	
	while (true) {
		timer::handle();
		radio::handle();
		usb::handle();
	}
}
