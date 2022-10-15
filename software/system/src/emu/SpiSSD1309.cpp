#include "SpiSSD1309.hpp"
#include "../Terminal.hpp"
#include <util.hpp>


SpiSSD1309::SpiSSD1309(int width, int height)
	: width(width), height(height)
{
	int size = width * height / 8;
	this->display = new uint8_t[size];
	array::fill(size, this->display, 0);
	this->displayBuffer = new uint8_t[width * height];

	// add to list of handlers
	Loop::handlers.add(*this);
}

SpiSSD1309::~SpiSSD1309() {
	delete [] this->display;
	delete [] this->displayBuffer;
}

Awaitable <SpiMaster::Parameters> SpiSSD1309::transfer(int writeCount, void const *writeData, int readCount, void *readData) {
	return {this->waitlist, nullptr, writeCount, writeData, readCount, readData};
}

void SpiSSD1309::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	bool command = writeCount < 0;
	writeCount &= 0x7fffffff;
	auto w = reinterpret_cast<uint8_t const *>(writeData);
	auto r = reinterpret_cast<uint8_t *>(readData);
	if (command) {
		// execute commands
		for (int i = 0; i < writeCount; ++i) {
			switch (w[i]) {
				// set contrast control
			case 0x81:
				this->displayContrast = w[++i];
				break;

				// entire display on
			case 0xA4:
				this->displayOn = false;
				break;
			case 0xA5:
				this->displayOn = true;
				break;

				// set normal/inverse display
			case 0xA6:
				this->displayInverse = false;
				break;
			case 0xA7:
				this->displayInverse = true;
				break;

				// set display on/off
			case 0xAE:
				this->displayEnabled = false;
				break;
			case 0xAF:
				this->displayEnabled = true;
				break;
			}
		}
	} else {
		// set data
		for (int i = 0; i < writeCount; ++i) {
			// copy byte (8 pixels in a column)
			this->display[page * this->width + this->column] = w[i];

			// increment column index
			this->column = (this->column == this->width - 1) ? 0 : this->column + 1;
			if (this->column == 0)
				this->page = this->page == (this->height / 8 - 1) ? 0 : this->page + 1;
		}
	}
}

void SpiSSD1309::handle(Gui &gui) {
	this->waitlist.resumeFirst([this](Parameters &p) {
		transferBlocking(p.writeCount, p.writeData, p.readCount, p.readData);
		return true;
	});

	// draw display on gui
	getDisplay(this->displayBuffer);
	gui.display(this->width, this->height, this->displayBuffer);
}

void SpiSSD1309::getDisplay(uint8_t *buffer) {
	uint8_t foreground = !this->displayEnabled ? 0 : this->displayContrast;
	uint8_t background = (!this->displayEnabled || this->displayOn) ? foreground : (48 * this->displayContrast) / 255;
	if (this->displayInverse)
		std::swap(foreground, background);

	int width = this->width;
	int height = this->height;
	for (int j = 0; j < height; ++j) {
		uint8_t *b = &buffer[width * j];
		for (int i = 0; i < width; ++i) {
			bool bit = (this->display[i + width * (j >> 3)] & (1 << (j & 7))) != 0;
			b[i] = bit ? foreground : background;
		}
	}
}
