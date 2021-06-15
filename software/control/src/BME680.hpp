#pragma once

#include <Coroutine.hpp>
#include <appConfig.hpp>
#include <cstdint>


/**
 * Controller for BME680 air sensor
 */
class BME680 {
public:
	
	enum State {
		CREATED,
		INITIALIZED,
		PARAMETERIZED
	};
	
	BME680() {}

	~BME680() {}

	/**
	 * Suspend execution using co_await until initialization is done
	 */
	Awaitable<> init();

	/**
	 * Returns the current state of the sensor
	 * @return current state
	 */
	State getState() {return this->state;}

	/**
	 * Suspend execution using co_await until parameters are set
	 * @param temperatureOversampling temperature oversampling, 1-5 (0 disabled)
	 * @param pressureOversampling pressure oversampling, 1-5 (0 disabled)
	 * @param filter iir filter for temperature and pressure, 0-7
	 * @param humidityOversampling humidity oversampling, 1-5 (0 disabled)
	 * @param heaterTemperature heater temperature for gas sensor, 200 to 400 degrees
	 * @param heaterDuration time between the beginning of the heat phase and the start of gas sensor resistance
	 * 	conversion in milliseconds
	 */
	Awaitable<> setParameters(int temperatureOversampling, int pressureOversampling, int filter,
		int humidityOversampling, int heaterTemperature, int heaterDuration);

	/**
	 * Suspend execution using co_await until measurement is done. Then use get-methods to obtain the measured values
	 */
	Awaitable<> measure();

	/**
	 * Get current temperature
	 * @return temperature in celsius
	 */
	float getTemperature() {return this->temperature;}

	/**
	 * Get current pressure
	 * @return pressure in ...
	 */
	float getPressure() {return this->pressure;}

	/**
	 * Get current humidity
	 * @return humidity in percent
	 */
	float getHumidity() {return this->humidity;}

	/**
	 * Get current gas resistance
	 * @return ras resistance in ohm
	 */
	float getGasResistance() {return this->gasResistance;}


protected:

	struct Parameters {
		uint8_t dummy;
		
		// odd alignment
		uint8_t cmd1; // read while register index is written
		int16_t t2; // 0x8A / 0x8B
		int8_t t3; // 0x8C
		uint8_t _8D;
		uint16_t p1; // 0x8E / 0x8F
		int16_t p2; // 0x90 / 0x91
		int8_t p3; // 0x92
		uint8_t _93;
		int16_t p4; // 0x94 / 0x95
		int16_t p5; // 0x96 / 0x97
		int8_t p7; // 0x98
		int8_t p6; // 0x99
		uint8_t _9A;
		uint8_t _9B;
		int16_t p8; // 0x9C / 0x9D
		int16_t p9; // 0x9E / 0x9F
		uint8_t p10; // 0xA0
		
		// odd alignment
		uint8_t cmd2; // read while register index is written
		uint16_t h2; // 0xE2<7:4> / 0xE1
		uint8_t _E3;
		int8_t h3; // 0xE4
		int8_t h4; // 0xE5
		int8_t h5; // 0xE6
		uint8_t h6; // 0xE7
		int8_t h7; // 0xE8
		uint16_t t1; // 0xE9 / 0xEA		
		int16_t gh2; // 0xEB / 0xEC
		int8_t gh1; // 0xED
		int8_t gh3; // 0xEE
		
		uint16_t h1; // 0xE2<3:0> / 0xE3
	};
/*
	void init1();
	void init2();
	void init3();
	void init4();
	void init5();

	void onMeasurementsReady();
*/
	// spi index
	static int const spiIndex = SPI_AIR_SENSOR;

	State state = CREATED;
	
	// read/write buffer
	uint8_t buffer[16];
	
	// calibration parameters
	Parameters par;
	int8_t res_heat_val;
	uint8_t res_heat_range;
	int8_t range_switching_error;
		
	// contents of register 74
	uint8_t _74;

	// measurement values
	float temperature;
	float pressure;
	float humidity;
	float gasResistance;
	
	
	//std::function<void ()> onReady;
	//bool busy;
};
