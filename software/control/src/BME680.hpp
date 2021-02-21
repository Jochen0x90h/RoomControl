#pragma once

#include <config.hpp>
#include <cstdint>
#include <functional>

/**
 * Display controller for SSD1309
 */
class BME680 {
public:
	
	/**
	 * Constructor. Initializes the sensor and leaves it in disabled state. Calls onReady when finished
	 */
	BME680(std::function<void ()> onReady);

	~BME680();

	/**
	 * Returns the number of send tasks that are in progress
	 */
	int getSendCount() {return this->sendCount;}

	/**
	 * Set parameters. Sets initialized state and calls onReady when finished
	 * @param temperatureOversampling temperature oversampling, 1-5 (0 disabled)
	 * @param pressureOversampling pressure oversampling, 1-5 (0 disabled)
	 * @param filter iir filter for temperature and pressure, 0-7
	 * @param humidityOversampling humidity oversampling, 1-5 (0 disabled)
	 * @param heaterTemperature heater temperature for gas sensor, 200 to 400 degrees
	 * @param heaterDuration time between the beginning of the heat phase and the start of gas sensor resistance
	 * 	conversion in milliseconds
	 */
	void setParameters(int temperatureOversampling, int pressureOversampling, int filter,
		int humidityOversampling, int heaterTemperature, int heaterDuration);

	/**
	 * Returns true if the sensor is initialized
	 */
	bool isInitialized() {return this->initialized;}

	/**
	 * Start a measurement. Call readMeasurements() a second later
	 */
	void startMeasurement();

	/**
	 * Read measurements from the sensor registers. Calls onReady when finished
	 */
	void readMeasurements();

	float getTemperature() {return this->temperature;}
	float getPressure() {return this->pressure;}
	float getHumidity() {return this->humidity;}
	float getGasResistance() {return this->gasResistance;}

	/**
	 * Called when the display is ready for the next action
	 */
	std::function<void ()> onReady;

protected:

	struct Parameters {
		uint8_t dummy;
		
		// odd alignment
		uint8_t cmd1; // read while register index is written
		uint16_t t2; // 0x8A / 0x8B
		uint8_t t3; // 0x8C
		uint8_t _8D;
		uint16_t p1; // 0x8E / 0x8F
		uint16_t p2; // 0x90 / 0x91
		uint8_t p3; // 0x92
		uint8_t _93;
		uint16_t p4; // 0x94 / 0x95
		uint16_t p5; // 0x96 / 0x97
		uint8_t p7; // 0x98
		uint8_t p6; // 0x99
		uint8_t _9A;
		uint8_t _9B;
		uint16_t p8; // 0x9C / 0x9D
		uint16_t p9; // 0x9E / 0x9F
		uint8_t p10; // 0xA0
		
		// odd alignment
		uint8_t cmd2; // read while register index is written
		uint16_t h2; // 0xE2<7:4> / 0xE1
		uint8_t _E3;
		uint8_t h3; // 0xE4
		uint8_t h4; // 0xE5
		uint8_t h5; // 0xE6
		uint8_t h6; // 0xE7
		uint8_t h7; // 0xE8
		uint16_t t1; // 0xE9 / 0xEA		
		uint16_t gh2; // 0xEB / 0xEC
		uint8_t gh1; // 0xED
		uint8_t gh3; // 0xEE
		
		uint16_t h1; // 0xE2<7:4> / 0xE3
	};

	void init1();
	void init2();
	void init3();
	void init4();
	void init5();

	void onMeasurementsReady();
	
	
	uint8_t sendCount = 0;
	
	// read/write buffer
	uint8_t buffer[16];
	
	// calibration parameters
	Parameters par;
	int8_t res_heat_val;
	uint8_t res_heat_range;
	int8_t range_switching_error;
	
	uint8_t _74;
	bool initialized = false;

	// measurements
	float temperature;
	float pressure;
	float humidity;
	float gasResistance;
};
