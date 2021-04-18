#include <BME680.hpp>
#include <timer.hpp>
#include <spi.hpp>
#include <usb.hpp>
#include <debug.hpp>
#include <loop.hpp>
#include <StringBuffer.hpp>
#include <Data.hpp>



// device descriptor
static const usb::DeviceDescriptor deviceDescriptor = {
	.bLength = sizeof(usb::DeviceDescriptor),
	.bDescriptorType = usb::DESCRIPTOR_DEVICE,
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


uint8_t timerId;
StringBuffer<128> string __attribute__((aligned(4)));


#define READ(reg) ((reg) | 0x80)
#define WRITE(reg) ((reg) & 0x7f)

constexpr int CHIP_ID = 0x61;
uint8_t buffer[129] __attribute__((aligned(4)));

/*
void getId() {
	// read chip id
	buffer[0] = READ(0xD0);
	spi::transfer(AIR_SENSOR_CS_PIN, buffer, 1, buffer, 2, []() {
		if (buffer[1] == CHIP_ID)
			debug::toggleGreenLed();
	});

	timer::start(timerId, timer::getTime() + 1s, []() {getId();});
}
*/
/*
void getRegisters() {
	debug::toggleRedLed();
	buffer[0] = READ(0);

	spi::transfer(AIR_SENSOR_CS_PIN, buffer, 1, buffer, 129, []() {
		usb::send(1, buffer + 1, 128, []() {
			debug::toggleGreenLed();
		});			
	});	
}
*/


void measure(BME680 &sensor);
void read(BME680 &sensor);
void getValues(BME680 &sensor);


void onInitialized(BME680 &sensor) {
	sensor.setParameters(
		2, 5, 2, // temperature and pressure oversampling and filter
		1, // humidity oversampling
		300, 100, // heater temperature and duration
		[&sensor]() {measure(sensor);});
	debug::setBlueLed(true);
}

void measure(BME680 &sensor) {
	sensor.startMeasurement();
	timer::start(timerId, timer::getTime() + 1s, [&sensor]() {read(sensor);});
	//debug::toggleGreenLed();
}

void read(BME680 &sensor) {
	//getRegisters();
	sensor.readMeasurements([&sensor]() {getValues(sensor);});
	
	timer::start(timerId, timer::getTime() + 59s, [&sensor]() {measure(sensor);});
}

void getValues(BME680 &sensor) {
	//debug::toggleRedLed();

	string = "Temperature: " + flt(sensor.getTemperature(), 1, 1) + "°C\n"
		+ "Pressure: " + flt(sensor.getPressure() / 100.0f, 1, 1) + "hPa\n"
		+ "Humidity: " + flt(sensor.getHumidity(), 1, 1) + "%\n"
		+ "Gas: " + flt(sensor.getGasResistance(), 1, 1) + "Ω\n";
	
	usb::send(1, string.data(), string.length(), []() {
		//debug::toggleBlueLed();
		debug::toggleRedLed();
	});	
}


int main(void) {
	loop::init();
	timer::init();
	spi::init();
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
			// enable bulk endpoints in 1 (keep control endpoint 0 enabled in both directions)
			usb::enableEndpoints(1 | (1 << 1), 1); 			
		
			//getRegisters();
		});
	debug::init();	
	
	timerId = timer::allocate();

	// test raw values
	//getId();
	
	// test sensor class
	BME680 sensor([&sensor]() {onInitialized(sensor);});

	loop::run();
}
