#include <BME680.hpp>
#include <timer.hpp>
#include <spi.hpp>
#include <usb.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <StringBuffer.hpp>



// device descriptor
static const usb::DeviceDescriptor deviceDescriptor = {
	.bLength = sizeof(usb::DeviceDescriptor),
	.bDescriptorType = usb::DescriptorType::DEVICE,
	.bcdUSB = 0x0200, // USB 2.0
	.bDeviceClass = 0xff, // no class
	.bDeviceSubClass = 0xff,
	.bDeviceProtocol = 1, // 0 = binary, 1 = text
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
		.bEndpointAddress = usb::IN | 1, // in 1 (tx)
		.bmAttributes = usb::EndpointType::BULK,
		.wMaxPacketSize = 64,
		.bInterval = 1 // polling interval
	}}
};


uint8_t timerId;
StringBuffer<128> string __attribute__((aligned(4)));


#define READ(reg) ((reg) | 0x80)
#define WRITE(reg) ((reg) & 0x7f)

constexpr int CHIP_ID = 0x61;
uint8_t buffer[129] __attribute__((aligned(4)));

// get chip id and output result on debug led's
Coroutine getId() {
	while (true) {
		// read chip id
		buffer[0] = READ(0xD0);
		co_await spi::transfer(SPI_AIR_SENSOR, 1, buffer, 2, buffer);
		
		// check chip id
		if (buffer[1] == CHIP_ID)
			debug::toggleGreenLed();
		else
			debug::toggleRedLed();

		// wait for 1s
		co_await timer::delay(1s);
	}
}

// read all registers and send to usb host
Coroutine getRegisters() {
	while (true) {
		// read upper page
		buffer[0] = READ(0);
		co_await spi::transfer(SPI_AIR_SENSOR, 1, buffer, 129, buffer);
		
		// send to usb host
		co_await usb::send(1, 128, buffer + 1);
		debug::toggleBlueLed();

		// wait for 5s
		co_await timer::delay(5s);
	}
}

// measure and send values to usb host
Coroutine measure() {
	BME680 sensor;

	co_await sensor.init();
	co_await sensor.setParameters(
		2, 5, 2, // temperature and pressure oversampling and filter
		1, // humidity oversampling
		300, 100); // heater temperature and duration
	debug::setBlueLed(true);

	while (true) {
		co_await sensor.measure();

		string = "Temperature: " + flt(sensor.getTemperature(), 1, 1) + "°C\n"
			+ "Pressure: " + flt(sensor.getPressure() / 100.0f, 1, 1) + "hPa\n"
			+ "Humidity: " + flt(sensor.getHumidity(), 1, 1) + "%\n"
			+ "Gas: " + flt(sensor.getGasResistance(), 1, 1) + "Ω\n";

		co_await usb::send(1, string.length(), string.data());
		debug::toggleRedLed();

		co_await timer::delay(10s);
	}
}


int main(void) {
	loop::init();
	timer::init();
	spi::init();
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
			// enable bulk endpoints in 1 (keep control endpoint 0 enabled in both directions)
			usb::enableEndpoints(1 | (1 << 1), 1); 			
		
			//getRegisters();
		},
		[](uint8_t bRequest, uint16_t wValue, uint16_t wIndex) {
			return false;
		});
	debug::init();	
	
	// test raw values
	//getId();
	
	measure();
	
	loop::run();
}
