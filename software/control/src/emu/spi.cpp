#include <spi.hpp>
#include <emu/global.hpp>


namespace spi {

// air sensor
constexpr int CHIP_ID = 0x61;
uint8_t airSensorBuffer[256] = {};
uint8_t *airSensor;

void doGui(Gui &gui, int &id) {
	// air sensor
	int temperature = gui.temperatureSensor(id++);
	if (temperature != -1) {
		float celsius = temperature / 20.0f + 273.15f;
		int value = int(celsius * 16384.0f * 5120.0f);
		spi::airSensorBuffer[24] = value << 4;
		spi::airSensorBuffer[23] = value >> 4;
		spi::airSensorBuffer[22] = value >> 12;
	}

	gui.newLine();
}


void init() {
	spi::airSensor = &spi::airSensorBuffer[128];
	
	// chip id
	spi::airSensorBuffer[0xD0] = CHIP_ID;
	
	// parameter t2
	spi::airSensorBuffer[0x8A] = 1;
}

void handle() {
}

bool transfer(int csPin, uint8_t const *writeData, int writeLength, uint8_t *readData, int readLength,
	std::function<void ()> onTransferred)
{
	if (csPin == AIR_SENSOR_CS_PIN) {
		if (writeData[0] & 0x80) {
			// read
			uint8_t addr = writeData[0] & 0x7f;
			readData[0] = 0;
			memcpy(readData + 1, airSensor, readLength - 1);
			
			// notify that we are ready
			asio::post(global::context, onTransferred);
		} else {
			// write
			for (int i = 0; i < writeLength - 1; i += 2) {
				uint8_t addr = writeData[i] & 0x7f;
				uint8_t data = writeData[i + 1];
				
				if (addr == 0x73) {
					// switch page
					airSensorBuffer[0x73] = data;
					airSensorBuffer[0xF3] = data;
					airSensor = &airSensorBuffer[(data & (1 << 4)) ? 0 : 128];
				}
				airSensor[addr] = data;
			}
		}
	}
	return true;
}

} // namespace spi
