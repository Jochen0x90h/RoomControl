#include "BME680.hpp"
#include <spi.hpp>
#include <timer.hpp>
#include <appConfig.hpp>
#include <assert.hpp>


// https://github.com/BoschSensortec/BME680_driver
// https://github.com/BoschSensortec/BME68x-Sensor-API

#define READ(reg) ((reg) | 0x80)
#define WRITE(reg) ((reg) & 0x7f)

constexpr int CHIP_ID = 0x61;


AwaitableCoroutine BME680::init(){
	// read chip id and check if it is ok
	this->buffer[0] = READ(0xD0);
	co_await spi::transfer(this->spiIndex, 1, this->buffer, 2, this->buffer);
	if (this->buffer[1] != CHIP_ID)
		co_return;
		
	// read calibration parameters 1
	this->buffer[0] = READ(0x8A);
	co_await spi::transfer(this->spiIndex, 1, this->buffer, 1 + 0xA1 - 0x8A, &this->par.cmd1);

	// read calibration parameters 2
	this->buffer[0] = READ(0xE1);
	co_await spi::transfer(this->spiIndex, 1, this->buffer, 1 + 0xEF - 0xE1, &this->par.cmd2);

	// fix calibration parameters h1 and h2
	{
		uint8_t e2 = this->par.h2 >> 8;
		this->par.h1 = (this->par._E3 << 4) | (e2 & 0x0f);
		this->par.h2 = ((this->par.h2 << 4) & 0xff0) | (e2 >> 4);
	}
	
	// switch register bank
	this->buffer[0] = WRITE(0x73);
	this->buffer[1] = 1 << 4;
	co_await spi::transfer(this->spiIndex, 2, this->buffer, 0, nullptr);

	// read res_heat_val, res_reat_range and range_switching_error
	this->buffer[0] = READ(0x00);
	co_await spi::transfer(this->spiIndex, 1, this->buffer, 6, this->buffer);
	{
		uint8_t const *buff = this->buffer + 1;
		this->res_heat_val = int8_t(buff[0x00]);
		this->res_heat_range = (buff[0x02] >> 4) & 3;
		this->range_switching_error = int8_t(buff[0x04]) >> 4;
	}
	
	this->state = INITIALIZED;
}

AwaitableCoroutine BME680::setParameters(int temperatureOversampling, int pressureOversampling, int filter,
	int humidityOversampling, int heaterTemperature, int heaterDuration)
{
	assert(this->state >= INITIALIZED);

	int i = 0;

	// humidity oversampling
	this->buffer[i++] = WRITE(0x72);
	this->buffer[i++] = (humidityOversampling & 3); // spi_3w_int_en stays off

	// temperature and pressure oversampling
	this->buffer[i++] = WRITE(0x74);
	this->buffer[i++] = this->_74 = ((temperatureOversampling & 3) << 5) | ((pressureOversampling & 3) << 2);

	// iir filter for temperature and pressure
	this->buffer[i++] = WRITE(0x75);
	this->buffer[i++] = (filter & 3) << 2; // spi_3w_en stays off

	// set heater resistance
	this->buffer[i++] = WRITE(0x5A);
	{
		float var1 = this->par.gh1 / 16.0f + 49.0f;
		float var2 = this->par.gh2 / 32768.0f * 0.0005f + 0.00235f;
		float var3 = this->par.gh3 / 1024.0f;
		float var4 = var1 * (1.0f + var2 * (heaterTemperature > 400 ? 400 : heaterTemperature));
		float var5 = var4 + var3 * getTemperature();
		this->buffer[i++] = uint8_t(3.4f * (var5 * 4.0f /
			((4.0f + this->res_heat_range) * (1.0f + this->res_heat_val * 0.002f)) - 25.0f));
	}

	// set heater duration in milliseconds
	this->buffer[i++] = WRITE(0x64);
	if (heaterDuration >= 0xfc0) {
		this->buffer[i++] = 0xff; // max duration
	} else {
		// calculate multiplication factor 1, 4, 16 or 64
		int factor = 0;
		while (heaterDuration > 0x3f) {
			heaterDuration = heaterDuration / 4;
			factor += 0x40;
		}
		this->buffer[i++] = uint8_t(heaterDuration + factor);
	}

	// run gas, profile index
	this->buffer[i++] = WRITE(0x71);
	this->buffer[i++] = (1 << 4) | 0;

	// transfer
	co_await spi::transfer(this->spiIndex, i, this->buffer, 0, nullptr);
	
	this->state = PARAMETERIZED;
}

AwaitableCoroutine BME680::measure() {
	assert(this->state == PARAMETERIZED);

	// start measurement
	this->buffer[0] = WRITE(0x74);
	this->buffer[1] = this->_74 | 1; // forced mode
	co_await spi::transfer(this->spiIndex, 2, this->buffer, 0, nullptr);

	// wait until measurement is ready
	co_await timer::delay(1s);

	// read measurements
	this->buffer[0] = READ(0x1D);
	co_await spi::transfer(this->spiIndex, 1, this->buffer, 16, this->buffer);
	{
		uint8_t const *buff = this->buffer + 1 - 0x1D;
		
		// state (5.3.5 Status registers)
		bool newData = (buff[0x1D] & (1 << 7)) != 0;
		bool measuring = (buff[0x1D] & (1 << 5)) != 0; // all values
		bool gasMeasuring = (buff[0x1D] & (1 << 6)) != 0; // only gas
		int gasMeasurementIndex = buff[0x1D] & 0x0f;
		bool gasValid = (buff[0x2B] & (1 << 5)) != 0;
		bool gasHeaterStable = (buff[0x2B] & (1 << 4)) != 0;
		
		// measurements (5.3.4 Data registers)
		int temperature = buff[0x22] * 4096 | buff[0x23] * 16 | buff[0x24] / 16;
		int pressure = buff[0x1F] * 4096 | buff[0x20] * 16 | buff[0x21] / 16;
		int humidity = buff[0x25] * 256 | buff[0x26];
		int gasResistance = buff[0x2A] * 4 | buff[0x2B] / 64;
		int gasRange = buff[0x2B] & 0x0f;
		if (newData) {
			// calculate temperature
			float t_fine;
			{
				float var1 = (temperature / 16384.0f - this->par.t1 / 1024.0f) * this->par.t2;
				float x = temperature / 131072.0f - this->par.t1 / 8192.0f;
				float var2 = x * x * this->par.t3 * 16.0f;
				t_fine = var1 + var2;
				this->temperature = t_fine / 5120.0f;
//std::cout << "BME680 temp " << this->temperature << std::endl;
			}
		
			// calculate pressure
			{
				float var1 = t_fine / 2.0f - 64000.0f;
				float var2 = var1 * var1 * this->par.p6 / 131072.0f;
				var2 = var2 + var1 * this->par.p5 * 2.0f;
				var2 = var2 / 4.0f + this->par.p4 * 65536.0f;
				var1 = (this->par.p3 * var1 * var1 / 16384.0f + this->par.p2 * var1) / 524288.0f;
				var1 = (1.0f + var1 / 32768.0f) * this->par.p1;
				float calc_pres = 1048576.0f - pressure;

				// Avoid exception caused by division by zero
				if (int(var1) != 0) {
					calc_pres = (calc_pres - var2 / 4096.0f) * 6250.0f / var1;
					var1 = this->par.p9 * calc_pres * calc_pres / 2147483648.0f;
					var2 = calc_pres * this->par.p8 / 32768.0f;
					float var3 = calc_pres * calc_pres * calc_pres / (256.0f * 256.0f * 256.0f)
						* this->par.p10 / 131072.0f;
					this->pressure = calc_pres + (var1 + var2 + var3 + this->par.p7 * 128.0f) / 16.0f;
				} else {
					this->pressure = 0;
				}
			}
			
			// calculate humidity
			{
				// compensated temperature data
				float temp_comp = t_fine / 5120.0f;
				float var1 = humidity - (this->par.h1 * 16.0f + this->par.h3 / 2.0f * temp_comp);
				float var2 = var1 * this->par.h2 / 262144.0f *
					(1.0f + this->par.h4 / 16384.0f * temp_comp + this->par.h5 / 1048576.0f * temp_comp * temp_comp);
				float var3 = this->par.h6 / 16384.0f;
				float var4 = this->par.h7 / 2097152.0f;
				this->humidity = var2 + (var3 + var4 * temp_comp) * var2 * var2;
				if (this->humidity > 100.0f)
					this->humidity = 100.0f;
				else if (this->humidity < 0.0f)
					this->humidity = 0.0f;
			}
			
			// calculate gas resistance
			{
				float gas_res_f = gasResistance;
				float gas_range_f = 1 << gasRange;
				const float lookup_k1_range[16] = {
					0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, -0.8f, 0.0f, 0.0f, -0.2f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f
				};
				const float lookup_k2_range[16] = {
					0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.7f, 0.0f, -0.8f, -0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
				};

				float var1 = 1340.0f + 5.0f * this->range_switching_error;
				float var2 = var1 * (1.0f + lookup_k1_range[gasRange] / 100.0f);
				float var3 = 1.0f + lookup_k2_range[gasRange] / 100.0f;
				this->gasResistance = 1.0f / (var3 * 0.000000125f * gas_range_f * ((gas_res_f - 512.0f) / var2 + 1.0f));
			}
		}
	}
}
