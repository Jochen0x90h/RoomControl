#include "../display.hpp"
#include <emu/global.hpp>
#include <config.hpp>


namespace display {

// display layout: rows of 8 pixels where each byte describes a column in each row
// this would be the layout of a 16x16 display where each '|' is one byte
// ||||||||||||||||
// ||||||||||||||||
int column; // column 0 to 127
int page; // page of 8 vertical pixels, 0 to 7
int displayContrast = 255;
bool displayOn = false; // all pixels on
bool displayInverse = false;
bool displayEnabled = false;
uint8_t display[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];

void getDisplay(uint8_t *buffer) {
	uint8_t foreground = !displayEnabled ? 0 : displayContrast;
	uint8_t background = (!displayEnabled || displayOn) ? foreground : (48 * displayContrast) / 255;
	if (displayInverse)
		std::swap(foreground, background);	
	
	int width = DISPLAY_WIDTH;
	int height = DISPLAY_HEIGHT;
	for (int j = 0; j < height; ++j) {
		uint8_t *b = &buffer[width * j];
		for (int i = 0; i < width; ++i) {
			// data layout: rows of 8 pixels where each byte describes a column in each row
			// this would be the layout of a 16x16 display where each '|' is one byte
			// ||||||||||||||||
			// ||||||||||||||||
			bool bit = (display[i + width * (j >> 3)] & (1 << (j & 7))) != 0;
			b[i] = bit ? foreground : background;
		}
	}
}


void init() {
}

bool send(uint8_t const* data, int commandLength, int dataLength, std::function<void ()> const &onSent) {

	// execute commands	
	for (int i = 0; i < commandLength; ++i) {
		switch (data[i]) {
		// set contrast control
		case 0x81:			
			displayContrast = data[++i];
			break;
		
		// entire display on
		case 0xA4:
			displayOn = false;
			break;
		case 0xA5:
			displayOn = true;
			break;
		
		// set normal/inverse display
		case 0xA6:
			displayInverse = false;
			break;
		case 0xA7:
			displayInverse = true;
			break;

		// set display on/off
		case 0xAE:
			displayEnabled = false;
			break;
		case 0xAF:
			displayEnabled = true;
			break;
		}		
	}

	// set data
	for (int i = 0; i < dataLength; ++i) {
		display[page * DISPLAY_WIDTH + column] = data[commandLength + i];

		column = (column == DISPLAY_WIDTH - 1) ? 0 : column + 1;
		if (column == 0)
			page = page == (DISPLAY_HEIGHT / 8 - 1) ? 0 : page + 1;
	}
	
	// notify that we are ready
	asio::post(global::context, onSent);
	
	return true;
}

} // namespace spi
