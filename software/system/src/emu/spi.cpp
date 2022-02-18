#include <spi.hpp>
#include <emu/loop.hpp>
#include <Queue.hpp>
#include <util.hpp>
#include <appConfig.hpp>
#include <boardConfig.hpp>
#include <fstream>


namespace spi {

// air sensor
// ----------
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


// fe-ram
// ------
static uint8_t framData[FRAM_SIZE + 1];
static std::fstream framFile;


// display
// -------

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
	uint8_t foreground = !spi::displayEnabled ? 0 : spi::displayContrast;
	uint8_t background = (!spi::displayEnabled || spi::displayOn) ? foreground : (48 * spi::displayContrast) / 255;
	if (spi::displayInverse)
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



struct Context {
	Waitlist<Parameters> waitlist;
};
Context contexts[SPI_CONTEXT_COUNT];


// event loop handler chain
loop::Handler nextHandler = nullptr;
void handle(Gui &gui) {
	// handle pending spi transfers
	for (int index = 0; index < SPI_CONTEXT_COUNT; ++index) {
		auto &context = spi::contexts[index];
		context.waitlist.resumeFirst([index](Parameters p) {

			if (index == SPI_EMU_AIR_SENSOR) {
				if (p.writeData[0] & 0x80) {
					// read
					int addr = p.writeData[0] & 0x7f;
					p.readData[0] = 0xff;
					memcpy(p.readData + 1, &spi::airSensor[addr], p.readLength - 1);
				} else {
					// write
					for (int i = 0; i < p.writeLength - 1; i += 2) {
						int addr = p.writeData[i] & 0x7f;
						uint8_t data = p.writeData[i + 1];
						
						if (addr == 0x73) {
							// switch page
							airSensorRegisters[0x73] = data;
							airSensorRegisters[0xF3] = data;
							airSensor = &airSensorRegisters[(data & (1 << 4)) ? 0 : 128];
						}
						airSensor[addr] = data;
					}
				}
			} else if (index == SPI_EMU_FRAM) {
				// emulate e.g. MR45V064B (16 bit address)
				
				uint8_t op = p.writeData[0];
				
				switch (op) {
				case FRAM_READ:
					{
						int addr = (p.writeData[1] << 8) | p.writeData[2];
						for (int i = 0; i < p.readLength - 3; ++i)
							p.readData[3 + i] = spi::framData[(addr + i) & (FRAM_SIZE - 1)];
					}
					break;
				case FRAM_WRITE:
					{
						int addr = (p.writeData[1] << 8) | p.writeData[2];
						for (int i = 0; i < p.writeLength - 3; ++i)
							spi::framData[(addr + i) & (FRAM_SIZE - 1)] = p.writeData[3 + i];

						// write to file
						spi::framFile.seekg(addr);
						spi::framFile.write(reinterpret_cast<char const *>(p.writeData + 3), p.writeLength - 3);
						spi::framFile.flush();
					}
					break;
				}
			} else if (index == SPI_EMU_DISPLAY) {
				if (p.command) {
					// execute commands
					for (int i = 0; i < p.writeLength; ++i) {
						switch (p.writeData[i]) {
						// set contrast control
						case 0x81:
							spi::displayContrast = p.writeData[++i];
							break;
						
						// entire display on
						case 0xA4:
							spi::displayOn = false;
							break;
						case 0xA5:
							spi::displayOn = true;
							break;
						
						// set normal/inverse display
						case 0xA6:
							spi::displayInverse = false;
							break;
						case 0xA7:
							spi::displayInverse = true;
							break;

						// set display on/off
						case 0xAE:
							spi::displayEnabled = false;
							break;
						case 0xAF:
							spi::displayEnabled = true;
							break;
						}
					}
				} else {
					// set data
					for (int i = 0; i < p.writeLength; ++i) {
						// copy byte (8 pixels in a column)
						spi::display[page * DISPLAY_WIDTH + spi::column] = p.writeData[i];

						// increment column index
						spi::column = (spi::column == DISPLAY_WIDTH - 1) ? 0 : spi::column + 1;
						if (spi::column == 0)
							spi::page = spi::page == (DISPLAY_HEIGHT / 8 - 1) ? 0 : spi::page + 1;
					}
				}
			}
			return true;
		});
	}
	
	// call next handler in chain
	spi::nextHandler(gui);
	
	//gui.newLine();

	if (SPI_EMU_AIR_SENSOR >= 0) {
		// draw temperature sensor on gui using random id
		auto temperature = gui.temperatureSensor(0xbc5032ad);
		if (temperature)
			setTemperature(*temperature);
	}
	if (SPI_EMU_DISPLAY >= 0) {
		// draw display on gui
		uint8_t displayBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
		getDisplay(displayBuffer);
		gui.display(DISPLAY_WIDTH, DISPLAY_HEIGHT, displayBuffer);
	}
}

void init(char const *fileName) {
	// check if already initialized
	if (spi::nextHandler != nullptr)
		return;

	// add to event loop handler chain
	spi::nextHandler = loop::addHandler(handle);

	
	// air sensor
	{
		spi::airSensor = &spi::airSensorRegisters[128];

		// set chip id
		spi::airSensorRegisters[0xD0] = CHIP_ID;
		
		// set parameter t2
		spi::airSensorRegisters[0x8A] = 0;
		spi::airSensorRegisters[0x8B] = 16384 >> 8;

		// set temperature
		setTemperature(20.0f);
	}
	
	// fram
	{
		// read fram contents from file and one excess byte to check size
		spi::framFile.open(fileName, std::fstream::in | std::fstream::binary);
		spi::framFile.read(reinterpret_cast<char *>(spi::framData), sizeof(spi::framData));
		spi::framFile.clear();
		size_t size = spi::framFile.tellg();
		spi::framFile.close();

		// check if size is ok
		if (size != FRAM_SIZE) {
			// no: erase emulated fram
			array::fill(spi::framData, 0xff);
			spi::framFile.open(fileName, std::fstream::trunc | std::fstream::in | std::fstream::out | std::fstream::binary);
			spi::framFile.write(reinterpret_cast<char *>(spi::framData), FRAM_SIZE);
			spi::framFile.flush();
		} else {
			// yes: open file and read emulated flash
			spi::framFile.open(fileName, std::fstream::in | std::fstream::out | std::fstream::binary);
			spi::framFile.read(reinterpret_cast<char *>(spi::framData), FRAM_SIZE);

		}
	}
	
	// display
	{
	}
}

Awaitable<Parameters> transfer(int index, int writeLength, uint8_t const *writeData, int readLength, uint8_t *readData, bool command)
{
	assert(spi::nextHandler != nullptr && uint(index) < SPI_CONTEXT_COUNT); // init() not called or index out of range
	auto &context = spi::contexts[index];

	return {context.waitlist, writeLength, writeData, readLength, readData, command};
}
/*
Awaitable<Parameters> write(int index, int writeLength, uint8_t const *writeData, bool command) {
	assert(spi::nextHandler != nullptr && uint(index) < SPI_CONTEXT_COUNT); // init() not called or index out of range
	auto &context = spi::contexts[index];

	return {context.waitlist, writeLength, writeData, int(command), nullptr};
}
*/
} // namespace spi
