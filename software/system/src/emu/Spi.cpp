#include "../Spi.hpp"
#include "Loop.hpp"
#include "../Terminal.hpp"
#include <Queue.hpp>
#include <util.hpp>
#include <appConfig.hpp>
#include <boardConfig.hpp>
#include <fstream>
#include <iostream>
//#include <StringOperators.hpp>


namespace Spi {

// air sensor
// ----------

#ifdef SPI_EMU_BME680

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
	Spi::airSensorRegisters[0x1D] = 1 << 7; // new data flag
	Spi::airSensorRegisters[0x24] = value << 4;
	Spi::airSensorRegisters[0x23] = value >> 4;
	Spi::airSensorRegisters[0x22] = value >> 12;
}

#endif


// display
// -------

#ifdef SPI_EMU_SSD1309

// display layout: rows of 8 pixels where each byte describes a column in each row.
// this would be the layout of a 16x16 display where each '|' is one byte:
// ||||||||||||||||
// ||||||||||||||||
int column; // column 0 to 127
int page; // page of 8 vertical pixels, 0 to 7
int displayContrast = 255;
bool displayOn = false; // all pixels on
bool displayInverse = false;
bool displayEnabled = false;
uint8_t display[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];

// get dipslay contents into an 8 bit grayscale image
void getDisplay(uint8_t *buffer) {
	uint8_t foreground = !Spi::displayEnabled ? 0 : Spi::displayContrast;
	uint8_t background = (!Spi::displayEnabled || Spi::displayOn) ? foreground : (48 * Spi::displayContrast) / 255;
	if (Spi::displayInverse)
		std::swap(foreground, background);
	
	int width = DISPLAY_WIDTH;
	int height = DISPLAY_HEIGHT;
	for (int j = 0; j < height; ++j) {
		uint8_t *b = &buffer[width * j];
		for (int i = 0; i < width; ++i) {
			bool bit = (display[i + width * (j >> 3)] & (1 << (j & 7))) != 0;
			b[i] = bit ? foreground : background;
		}
	}
}

#endif


// fe-ram
// ------

#ifdef SPI_EMU_MR45V064B

static uint8_t framData[FRAM_SIZE + 1];
static std::fstream framFile;

#endif


// relay driver
// ------------

#ifdef SPI_EMU_MPQ6526

Gui::LightState relayStates[4];

void updateState(int index, uint16_t data, int i1, int i2) {
    int x = (data >> i1) & 3;
    int y = (data >> i2) & 3;

    // high and low side of a half-bridge should never be on at the same time
    assert(x != 3 && y != 3);

    if ((x == 1 && y == 2) || (x == 2 && y == 1)) {
        auto state = x == 2 ? Gui::LightState::ON : Gui::LightState::OFF;
        if (state != Spi::relayStates[index])
            std::cout << index << (state == Gui::LightState::ON ? ":on " : ":off ");
        Spi::relayStates[index] = state;
    }
}

#endif


struct Context {
	Waitlist<Parameters> waitlist;
};
Context contexts[SPI_CONTEXT_COUNT];


// event loop handler chain
Loop::Handler nextHandler = nullptr;
void handle(Gui &gui) {
	// handle pending Spi transfers
	for (int index = 0; index < SPI_CONTEXT_COUNT; ++index) {
		auto &context = Spi::contexts[index];
		context.waitlist.resumeFirst([index](Parameters p) {
			int writeLength = p.writeLength & 0x7fffffff;
			bool command = p.writeLength < 0;
			
#ifdef SPI_EMU_BME680
			if (index == SPI_EMU_BME680) {
				auto writeData = (uint8_t const *)p.writeData;
				auto readData = (uint8_t *)p.readData;
				if (writeData[0] & 0x80) {
					// read
					int addr = writeData[0] & 0x7f;
					readData[0] = 0xff;
					memcpy(readData + 1, &Spi::airSensor[addr], p.readLength - 1);
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
			}
#endif
			
#ifdef SPI_EMU_SSD1309
			if (index == SPI_EMU_SSD1309) {
				auto writeData = (uint8_t const *)p.writeData;
				auto readData = (uint8_t *)p.readData;
				if (command) {
					// execute commands
					for (int i = 0; i < writeLength; ++i) {
						switch (writeData[i]) {
						// set contrast control
						case 0x81:
							Spi::displayContrast = writeData[++i];
							break;
						
						// entire display on
						case 0xA4:
							Spi::displayOn = false;
							break;
						case 0xA5:
							Spi::displayOn = true;
							break;
						
						// set normal/inverse display
						case 0xA6:
							Spi::displayInverse = false;
							break;
						case 0xA7:
							Spi::displayInverse = true;
							break;

						// set display on/off
						case 0xAE:
							Spi::displayEnabled = false;
							break;
						case 0xAF:
							Spi::displayEnabled = true;
							break;
						}
					}
				} else {
					// set data
					for (int i = 0; i < writeLength; ++i) {
						// copy byte (8 pixels in a column)
						Spi::display[page * DISPLAY_WIDTH + Spi::column] = writeData[i];

						// increment column index
						Spi::column = (Spi::column == DISPLAY_WIDTH - 1) ? 0 : Spi::column + 1;
						if (Spi::column == 0)
							Spi::page = Spi::page == (DISPLAY_HEIGHT / 8 - 1) ? 0 : Spi::page + 1;
					}
				}
			}
#endif
			
#ifdef SPI_EMU_MR45V064B
			if (index == SPI_EMU_MR45V064B) {
				// emulate MR45V064B (16 bit address)
				auto writeData = (uint8_t const *)p.writeData;
				auto readData = (uint8_t *)p.readData;
				
				uint8_t op = writeData[0];
				
				switch (op) {
				case FRAM_READ:
					{
						int addr = (writeData[1] << 8) | writeData[2];
						for (int i = 0; i < p.readLength - 3; ++i)
							readData[3 + i] = Spi::framData[(addr + i) & (FRAM_SIZE - 1)];
					}
					break;
				case FRAM_WRITE:
					{
						int addr = (writeData[1] << 8) | writeData[2];
						for (int i = 0; i < writeLength - 3; ++i)
							Spi::framData[(addr + i) & (FRAM_SIZE - 1)] = writeData[3 + i];

						// write to file
						Spi::framFile.seekg(addr);
						Spi::framFile.write(reinterpret_cast<char const *>(writeData + 3), writeLength - 3);
						Spi::framFile.flush();
					}
					break;
				}
			}
#endif

#ifdef SPI_EMU_MPQ6526
			if (index == SPI_EMU_MPQ6526) {
				// emulate MPQ6526 hex half bridge driver
				uint16_t writeData = *(uint16_t const *)p.writeData;
				uint16_t &readData = *(uint16_t *)p.readData;
				
				bool driverEnable = (writeData & 0x8000) != 0;
				if (driverEnable) {
                    std::cout << "relays ";
                    updateState(0, writeData, 1, 3);
					updateState(1, writeData, 5, 3);
					updateState(2, writeData, 7, 9);
					updateState(3, writeData, 11, 9);
                    std::cout << std::endl;
				}
				
				// todo
				//readData = 0;
			}
#endif
			
			return true;
		});
	}
	
	// call next handler in chain
	Spi::nextHandler(gui);
	
	//gui.newLine();

#ifdef SPI_EMU_BME680
	{
		// draw temperature sensor on gui using random id
		auto temperature = gui.temperatureSensor(0xbc5032ad);
		if (temperature)
			setTemperature(*temperature);
	}
#endif
	
#ifdef SPI_EMU_SSD1309
	{
		// draw display on gui
		uint8_t displayBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
		getDisplay(displayBuffer);
		gui.display(DISPLAY_WIDTH, DISPLAY_HEIGHT, displayBuffer);
	}
#endif
	
#ifdef SPI_EMU_MPQ6526
	{
		// draw relay driver on gui				
		gui.light(Spi::relayStates[0]);
		gui.light(Spi::relayStates[1]);
		gui.light(Spi::relayStates[2]);
		gui.light(Spi::relayStates[3]);
	}
#endif
}

void init(char const *fileName) {
	// check if already initialized
	if (Spi::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	Spi::nextHandler = Loop::addHandler(handle);

	
	// air sensor
#ifdef SPI_EMU_BME680
	{
		Spi::airSensor = &Spi::airSensorRegisters[128];

		// set chip id
		Spi::airSensorRegisters[0xD0] = CHIP_ID;
		
		// set parameter t2
		Spi::airSensorRegisters[0x8A] = 0;
		Spi::airSensorRegisters[0x8B] = 16384 >> 8;

		// set temperature
		setTemperature(20.0f);
	}
#endif

	// fram
#ifdef SPI_EMU_MR45V064B
	{
		// read fram contents from file and one excess byte to check size
		Spi::framFile.open(fileName, std::fstream::in | std::fstream::binary);
		Spi::framFile.read(reinterpret_cast<char *>(Spi::framData), sizeof(Spi::framData));
		Spi::framFile.clear();
		size_t size = Spi::framFile.tellg();
		Spi::framFile.close();

		// check if size is ok
		if (size != FRAM_SIZE) {
			// no: erase emulated fram
			array::fill(Spi::framData, 0xff);
			Spi::framFile.open(fileName, std::fstream::trunc | std::fstream::in | std::fstream::out | std::fstream::binary);
			Spi::framFile.write(reinterpret_cast<char *>(Spi::framData), FRAM_SIZE);
			Spi::framFile.flush();
		} else {
			// yes: open file and read emulated flash
			Spi::framFile.open(fileName, std::fstream::in | std::fstream::out | std::fstream::binary);
			Spi::framFile.read(reinterpret_cast<char *>(Spi::framData), FRAM_SIZE);

		}
	}
#endif
}

Awaitable<Parameters> transfer(int index, int writeLength, void const *writeData, int readLength, void *readData) {
    // check if Spi::init() was called and index in range
    assert(Spi::nextHandler != nullptr && uint(index) < SPI_CONTEXT_COUNT);
	auto &context = Spi::contexts[index];

	return {context.waitlist, writeLength, writeData, readLength, readData};
}

} // namespace Spi
