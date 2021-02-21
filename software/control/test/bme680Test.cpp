#include <timer.hpp>
#include <spi.hpp>
#include <BME680.hpp>
#include <debug.hpp>


bool blink = false;

void read(BME680 &sensor);


void measure(BME680 &sensor) {
	if (sensor.isInitialized()) {
		sensor.startMeasurement();
		timer::start(0, timer::getTime() + 1s, [&sensor]() {read(sensor);});
	} else {
		timer::start(0, timer::getTime() + 10s, [&sensor]() {measure(sensor);});	
	}
}

void read(BME680 &sensor) {
	sensor.readMeasurements(); // onReady -> getValues
	timer::start(0, timer::getTime() + 9s, [&sensor]() {measure(sensor);});
}

void getValues(BME680 &sensor) {
	sensor.getTemperature();
}

void initialized(BME680 &sensor) {
	sensor.setParameters(
		2, 5, 2, // temperature and pressure oversampling and filter
		1, // humidity oversampling
		300, 100); // heater temperature and duration

	// set onReady to getValues 
	sensor.onReady = [&sensor]() {getValues(sensor);};
}

int main(void) {
	timer::init();
	spi::init();
	debug::init();	
	
	BME680 sensor([&sensor]() {initialized(sensor);});
	
	// start measurement
	measure(sensor);
			
	while (true) {
		timer::handle();
		spi::handle();
	}
}
