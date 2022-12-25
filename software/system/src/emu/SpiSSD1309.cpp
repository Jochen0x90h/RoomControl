#include "SpiSSD1309.hpp"
#include "../Terminal.hpp"
#include <util.hpp>


SpiSSD1309::SpiSSD1309(int width, int height)
	: width(width), height(height)
{
	int size = width * height / 8;
	this->data = new uint8_t[size];
	array::fill(size, this->data, 0);
	this->displayBuffer = new uint8_t[width * height];

	// add to list of handlers
	loop::handlers.add(*this);
}

SpiSSD1309::~SpiSSD1309() {
	delete [] this->data;
	delete [] this->displayBuffer;
}

Awaitable <SpiMaster::Parameters> SpiSSD1309::transfer(int writeCount, void const *writeData, int readCount, void *readData) {
	return {this->waitlist, nullptr, writeCount, writeData, readCount, readData};
}

void SpiSSD1309::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	auto w = reinterpret_cast<uint8_t const *>(writeData);

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
}

void SpiSSD1309::handle(Gui &gui) {
	this->waitlist.resumeFirst([this](Parameters &p) {
		if (p.config == nullptr) {
			transferBlocking(p.writeCount, p.writeData, p.readCount, p.readData);
		} else {
			auto &d = *reinterpret_cast<Data *>(p.config);
			d.transferBlocking(p.writeCount, p.writeData, p.readCount, p.readData);
		}
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
			bool bit = (this->data[i + width * (j >> 3)] & (1 << (j & 7))) != 0;
			b[i] = bit ? foreground : background;
		}
	}
}


Awaitable<SpiMaster::Parameters> SpiSSD1309::Data::transfer(int writeCount, const void *writeData, int readCount, void *readData) {
	return {this->display.waitlist, this, writeCount, writeData, readCount, readData};
}

void SpiSSD1309::Data::transferBlocking(int writeCount, const void *writeData, int readCount, void *readData) {
	auto &d = this->display;
	auto w = reinterpret_cast<uint8_t const *>(writeData);

	// set data
	for (int i = 0; i < writeCount; ++i) {
		// copy byte (8 pixels in a column)
		d.data[d.page * d.width + d.column] = w[i];

		// increment column index
		d.column = (d.column == d.width - 1) ? 0 : d.column + 1;
		if (d.column == 0)
			d.page = d.page == (d.height / 8 - 1) ? 0 : d.page + 1;
	}
}
