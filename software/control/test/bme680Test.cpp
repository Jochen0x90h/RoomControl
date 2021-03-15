#include <timer.hpp>
#include <spi.hpp>
#include <BME680.hpp>
#include <debug.hpp>


#define READ(reg) ((reg) | 0x80)
#define WRITE(reg) ((reg) & 0x7f)

constexpr int CHIP_ID = 0x61;

uint8_t buffer[10];


void getId() {
	// read chip id
	buffer[0] = READ(0xD0);
	spi::transfer(AIR_SENSOR_CS_PIN, buffer, 1, buffer, 2, []() {
		if (buffer[1] == CHIP_ID)
			debug::toggleGreenLed();
	});

	timer::start(0, timer::getTime() + 1s, []() {getId();});
}


void measure(BME680 &sensor);
void read(BME680 &sensor);
void getValues(BME680 &sensor);


void onInitialized(BME680 &sensor) {
	sensor.setParameters(
		2, 5, 2, // temperature and pressure oversampling and filter
		1, // humidity oversampling
		300, 100, // heater temperature and duration
		[&sensor]() {measure(sensor);});
	//debug::setRedLed(true);
}

void measure(BME680 &sensor) {
	sensor.startMeasurement();
	timer::start(0, timer::getTime() + 1s, [&sensor]() {read(sensor);});
	//debug::toggleGreenLed();
}

void read(BME680 &sensor) {
	sensor.readMeasurements([&sensor]() {getValues(sensor);});
	timer::start(0, timer::getTime() + 9s, [&sensor]() {measure(sensor);});
}

void getValues(BME680 &sensor) {
	//debug::toggleRedLed();

	float temperature = sensor.getTemperature();
	int t = int(temperature * 10.0f);
	debug::setRedLed(t & 1);
	debug::setGreenLed((t / 10) & 1);
	
	buffer[0] = t;
	buffer[1] = t >> 8;
	buffer[2] = t >> 16;
	buffer[3] = t >> 24;

	spi::transfer(FERAM_CS_PIN, buffer, 4, nullptr, 0, []() {});
}


int main(void) {
	timer::init();
	spi::init();
	debug::init();	
	
	//getId();

	BME680 sensor([&sensor]() {onInitialized(sensor);});
				
	while (true) {
		timer::handle();
		spi::handle();
	}
}
