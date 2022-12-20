#include <Loop.hpp>
#include <Timer.hpp>
#include <UsbDevice.hpp>
#include <Debug.hpp>
#include <Coroutine.hpp>
#include <util.hpp>
//#include "nrf52/nrf52.hpp"
//#include "nrf52/FlashImpl.hpp"
#include <boardConfig.hpp>


// Test for USB device.
// Flash this onto the device, then run UsbTestHost on a PC


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
	struct usb::EndpointDescriptor endpoints[2];
};

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


constexpr int bufferSize = 128;
uint8_t buffer[bufferSize];// __attribute__((aligned(4)));

//FlashImpl flash{0xe0000 - 0x20000, 2, 4096};
//uint8_t writeData[] = {0x12, 0x34, 0x56, 0x78, 0x9a};

// echo data from host
Coroutine echo(UsbDevice &usb) {
	while (true) {
		// receive data from host
		int length = bufferSize;
		co_await usb.receive(1, length, buffer);

		// set green led to indicate processing
		Debug::setGreenLed();

		// check received data
		bool error = false;
		for (int i = 0; i < length; ++i) {
			if (buffer[i] != length + i)
				error = true;
		}
		if (error)
			Debug::setColor(Debug::RED);
		
		// send data back to host
		co_await usb.send(1, length, buffer);

		/*
		// debug: send nrf52840 chip id
		uint32_t variant = NRF_FICR->INFO.VARIANT;
		buffer[0] = variant >> 24;
		buffer[1] = variant >> 16;
		buffer[2] = variant >> 8;
		buffer[3] = variant;
		co_await UsbDevice::send(1, 4, buffer);

		// debug: send nrf52840 interrupt priorities
		buffer[0] = getInterruptPriority(USBD_IRQn);
		buffer[1] = getInterruptPriority(RADIO_IRQn);
		buffer[2] = getInterruptPriority(TIMER0_IRQn);
		buffer[3] = getInterruptPriority(RNG_IRQn);
		co_await UsbDevice::send(1, 4, buffer);

		// debug: send nrf5252840 UICR.NRFFW[0]
		uint32_t offset = NRF_UICR->NRFFW[0];
		buffer[0] = offset;
		buffer[1] = offset >> 8;
		buffer[2] = offset >> 16;
		buffer[3] = offset >> 24;
		co_await UsbDevice::send(1, 4, buffer);

		// debug: read from flash
		flash.readBlocking(0, 4, buffer);
		buffer[4] = array::equal(4, writeData, buffer);
		co_await UsbDevice::send(1, 5, buffer);
		*/

		// clear green led and toggle blue led to indicate activity
		Debug::clearGreenLed();
		Debug::toggleBlueLed();
	}
}

int main() {
	Loop::init();
	Timer::init();

	//UsbDevice::init(
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
			// enable bulk endpoints 1 in and 1 out (keep control endpoint 0 enabled)
			Debug::setGreenLed(true);
			usb.enableEndpoints(1 | (1 << 1), 1 | (1 << 1));
		},
		[](uint8_t bRequest, uint16_t wValue, uint16_t wIndex) {
			switch (Request(bRequest)) {
			case Request::RED:
				Debug::setRedLed(wValue != 0);
				// debug: erase flash and write
				//flash.eraseSectorBlocking(0);
				//flash.writeBlocking(0, 4, writeData + 1);
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

	// start to receive from usb host
	echo(usb);

	Loop::run();
}
