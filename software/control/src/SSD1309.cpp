#include "SSD1309.hpp"
#include <display.hpp>


Awaitable<> SSD1309::init() {
	// command unlock
	this->command[0] = 0xFD;
	this->command[1] = 0x12;
	co_await display::send(2, 0, this->command);

	// set display off
	this->command[0] = 0xAE;
	co_await display::send(1, 0, this->command);

	// set display clock divide ratio / oscillator frequency
	this->command[0] = 0xD5;
	this->command[1] = 0xA0;
	co_await display::send(2, 0, this->command);

	// set multiplex ratio
	this->command[0] = 0xA8;
	this->command[1] = 0x3F;
	co_await display::send(2, 0, this->command);

	// set display offset
	this->command[0] = 0xD3;
	this->command[1] = 0x00;
	co_await display::send(2, 0, this->command);

	// set display start line
	this->command[0] = 0x40;
	co_await display::send(1, 0, this->command);

	// set segment remap
	this->command[0] = 0xA1;
	co_await display::send(1, 0, this->command);

	// set com output scan direction
	this->command[0] = 0xC8;
	co_await display::send(1, 0, this->command);

	// set com pins hardware configuration
	this->command[0] = 0xDA;
	this->command[1] = 0x12;
	co_await display::send(2, 0, this->command);

	// set current control
	this->command[0] = 0x81;
	this->command[1] = 0xDF;
	co_await display::send(2, 0, this->command);

	// set pre-charge period
	this->command[0] = 0xD9;
	this->command[1] = 0x82;
	co_await display::send(2, 0, this->command);

	// set vcomh deselect level
	this->command[0] = 0xDB;
	this->command[1] = 0x34;
	co_await display::send(2, 0, this->command);

	// set entire display to normal
	this->command[0] = 0xA4;
	co_await display::send(1, 0, this->command);

	// set inverse display to normal
	this->command[0] = 0xA6;
	co_await display::send(1, 0, this->command);
}

Awaitable<> SSD1309::enable() {
	display::enableVcc(true);
	this->command[0] = 0xAF;
	co_await display::send(1, 0, this->command);
	this->enabled = true;
}

Awaitable<> SSD1309::disable() {
	this->command[0] = 0xAE;
	co_await display::send(1, 0, this->command);
	this->enabled = false;
	display::enableVcc(false);
}

Awaitable<> SSD1309::setContrast(uint8_t contrast) {
	this->command[0] = 0x81;
	this->command[1] = contrast;
	co_await display::send(2, 0, this->command);
}

Awaitable<> SSD1309::set(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> const &bitmap) {
	co_await display::send(0, array::size(bitmap.data), bitmap.data);
}
