#include "SSD1309.hpp"


AwaitableCoroutine SSD1309::init() {
	// command unlock
	this->command[0] = 0xFD;
	this->command[1] = 0x12;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set display off
	this->command[0] = 0xAE;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);

	// set display clock divide ratio / oscillator frequency
	this->command[0] = 0xD5;
	this->command[1] = 0xA0;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set multiplex ratio
	this->command[0] = 0xA8;
	this->command[1] = 0x3F;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set display offset
	this->command[0] = 0xD3;
	this->command[1] = 0x00;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set display start line
	this->command[0] = 0x40;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);

	// set segment remap
	this->command[0] = 0xA1;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);

	// set com output scan direction
	this->command[0] = 0xC8;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);

	// set com pins hardware configuration
	this->command[0] = 0xDA;
	this->command[1] = 0x12;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set current control
	this->command[0] = 0x81;
	this->command[1] = 0xDF;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set pre-charge period
	this->command[0] = 0xD9;
	this->command[1] = 0x82;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set vcomh deselect level
	this->command[0] = 0xDB;
	this->command[1] = 0x34;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);

	// set entire display to normal
	this->command[0] = 0xA4;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);

	// set inverse display to normal
	this->command[0] = 0xA6;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);
}

AwaitableCoroutine SSD1309::enable() {
	//display::enableVcc(true);
	this->command[0] = 0xAF;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);
	this->enabled = true;
}

AwaitableCoroutine SSD1309::disable() {
	this->command[0] = 0xAE;
	co_await spi::write(SPI_DISPLAY, 1, this->command, true);
	this->enabled = false;
	//display::enableVcc(false);
}

AwaitableCoroutine SSD1309::setContrast(uint8_t contrast) {
	this->command[0] = 0x81;
	this->command[1] = contrast;
	co_await spi::write(SPI_DISPLAY, 2, this->command, true);
}
