#include <display.hpp>
#include <emu/loop.hpp>
#include <appConfig.hpp>


namespace display {

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
	uint8_t foreground = !display::displayEnabled ? 0 : display::displayContrast;
	uint8_t background = (!display::displayEnabled || display::displayOn) ? foreground : (48 * display::displayContrast) / 255;
	if (display::displayInverse)
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

Waitlist<Parameters> waitlist;


// event loop handler chain
loop::Handler nextHandler;
void handle(Gui &gui) {
	display::waitlist.resumeAll([](Parameters p) {
		// execute commands
		for (int i = 0; i < p.commandLength; ++i) {
			switch (p.data[i]) {
			// set contrast control
			case 0x81:
				display::displayContrast = p.data[++i];
				break;
			
			// entire display on
			case 0xA4:
				display::displayOn = false;
				break;
			case 0xA5:
				display::displayOn = true;
				break;
			
			// set normal/inverse display
			case 0xA6:
				display::displayInverse = false;
				break;
			case 0xA7:
				display::displayInverse = true;
				break;

			// set display on/off
			case 0xAE:
				display::displayEnabled = false;
				break;
			case 0xAF:
				display::displayEnabled = true;
				break;
			}
		}

		// set data
		for (int i = 0; i < p.dataLength; ++i) {
			// copy byte (8 pixels in a column)
			display::display[page * DISPLAY_WIDTH + display::column] = p.data[p.commandLength + i];

			// increment column index
			display::column = (display::column == DISPLAY_WIDTH - 1) ? 0 : display::column + 1;
			if (display::column == 0)
				display::page = display::page == (DISPLAY_HEIGHT / 8 - 1) ? 0 : display::page + 1;
		}
		
		return true;
	});

	// call next handler in chain
	display::nextHandler(gui);

	// draw display on gui
	uint8_t displayBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
	getDisplay(displayBuffer);
	gui.display(DISPLAY_WIDTH, DISPLAY_HEIGHT, displayBuffer);
}

void init() {
	// add to event loop handler chain
	display::nextHandler = loop::addHandler(handle);
}

Awaitable<Parameters> send(int commandLength, int dataLength, uint8_t const *data) {
	return {display::waitlist, commandLength, dataLength, data};
}

} // namespace spi
