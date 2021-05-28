#include <spi.hpp>
#include <emu/loop.hpp>
#include <util.hpp>
#include <appConfig.hpp>
#include <sysConfig.hpp>
#include <iostream>


namespace spi {

// air sensor
constexpr int CHIP_ID = 0x61;

uint8_t airSensorRegisters[256] = {};
/*
uint8_t airSensorRegisters[256] = {
	0x29, 0xaa, 0x16, 0xcb, 0x13, 0x06, 0x48, 0x21, 0x00, 0x00, 0x01, 0x38, 0x14, 0x04, 0x02, 0xa0, // 0x
	0xa0, 0x00, 0x04, 0xff, 0xf0, 0x00, 0x20, 0x00, 0x1f, 0x7f, 0x1f, 0x10, 0x00, 0x80, 0x00, 0x54, // 1x
	0xf6, 0xf0, 0x79, 0x72, 0x00, 0x52, 0xaa, 0x80, 0x00, 0x00, 0x11, 0xba, 0x00, 0x04, 0x00, 0x00, // 2x
	0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00, // 3x
	0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, // 4x
	0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, // 5x
	0x00, 0x00, 0x00, 0x00, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 6x
	0x10, 0x10, 0x01, 0x10, 0x44, 0x08, 0x00, 0x00, 0x0f, 0x04, 0xfe, 0x16, 0x9b, 0x08, 0x10, 0x00, // 7x: 73.4 = page
	
	0x8c, 0x6f, 0x89, 0x3e, 0x1a, 0x3c, 0x2e, 0x06, 0xb0, 0xc0, 0xea, 0x66, 0x03, 0x00, 0x35, 0x91, // 8x: 8A:8B = t2
	0x71, 0xd7, 0x58, 0x00, 0xaf, 0x16, 0xf0, 0xff, 0x37, 0x1e, 0x00, 0x00, 0x8e, 0xef, 0x5c, 0xf8, // 9x
	0x1e, 0x9a, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0xc0, // Ax
	0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x60, 0x02, 0x00, 0x01, 0x00, 0xc2, 0x1f, 0x60, 0x03, 0x00, // Bx
	0x00, 0x87, 0x00, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Cx
	0x61, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x10, 0x40, 0x00, // Dx: D0 = chip id
	0x00, 0x3f, 0x4e, 0x2f, 0x00, 0x2d, 0x14, 0x78, 0x9c, 0x56, 0x66, 0x19, 0xf3, 0xe8, 0x12, 0xc2, // Ex
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x80  // Fx: F3.4 = page
};
*/
// pointer to lower or upper page in the air sensor registers
uint8_t *airSensor;

void setTemperature(float celsius) {
	int value = int(celsius * 5120.0f);
	spi::airSensorRegisters[0x1D] = 1 << 7; // new data flag
	spi::airSensorRegisters[0x24] = value << 4;
	spi::airSensorRegisters[0x23] = value >> 4;
	spi::airSensorRegisters[0x22] = value >> 12;
}

void init() {
	spi::airSensor = &spi::airSensorRegisters[128];

	// set chip id
	spi::airSensorRegisters[0xD0] = CHIP_ID;
	
	// set parameter t2
	spi::airSensorRegisters[0x8A] = 0;
	spi::airSensorRegisters[0x8B] = 16384 >> 8;

	// set temperature
	setTemperature(20.0f);
}

bool transfer(int index, uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> const &onTransferred)
{
	assert(uint(index) < array::size(SPI_CS_PINS));

	if (index == SPI_AIR_SENSOR) {
		if (writeData[0] & 0x80) {
			// read
			int addr = writeData[0] & 0x7f;
			readData[0] = 0xff;
			memcpy(readData + 1, &spi::airSensor[addr], readLength - 1);
		} else {
			// write
			for (int i = 0; i < writeLength - 1; i += 2) {
				int addr = writeData[i] & 0x7f;
				uint8_t data = writeData[i + 1];
				
				if (addr == 0x73) {
					// switch page
					airSensorRegisters[0x73] = data;
					airSensorRegisters[0xF3] = data;
					airSensor = &airSensorRegisters[(data & (1 << 4)) ? 0 : 128];
				}
				airSensor[addr] = data;
			}
		}

		// notify that we are ready
		asio::post(loop::context, onTransferred);
	}
	return true;
}

} // namespace spi
