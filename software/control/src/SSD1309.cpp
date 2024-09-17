#include "SSD1309.hpp"


AwaitableCoroutine SSD1309::init() {
	uint8_t command[2];

	// command unlock
	command[0] = 0xFD;
	command[1] = 0x12;
	co_await this->spi.command.write(2, command);

	// set display off
	command[0] = 0xAE;
	co_await this->spi.command.write(1, command);

	// set display clock divide ratio / oscillator frequency
	command[0] = 0xD5;
	command[1] = 0xA0;
	co_await this->spi.command.write(2, command);

	// set multiplex ratio
	command[0] = 0xA8;
	command[1] = 0x3F;
	co_await this->spi.command.write(2, command);

	// set display offset
	command[0] = 0xD3;
	command[1] = 0x00;
	co_await this->spi.command.write(2, command);

	// set display start line
	command[0] = 0x40;
	co_await this->spi.command.write(1, command);

	// set segment remap
	command[0] = 0xA1;
	co_await this->spi.command.write(1, command);

	// set com output scan direction
	command[0] = 0xC8;
	co_await this->spi.command.write(1, command);

	// set com pins hardware configuration
	command[0] = 0xDA;
	command[1] = 0x12;
	co_await this->spi.command.write(2, command);

	// set current control
	command[0] = 0x81;
	command[1] = 0xDF;
	co_await this->spi.command.write(2, command);

	// set pre-charge period
	command[0] = 0xD9;
	command[1] = 0x82;
	co_await this->spi.command.write(2, command);

	// set vcomh deselect level
	command[0] = 0xDB;
	command[1] = 0x34;
	co_await this->spi.command.write(2, command);

	// set entire display to normal
	command[0] = 0xA4;
	co_await this->spi.command.write(1, command);

	// set inverse display to normal
	command[0] = 0xA6;
	co_await this->spi.command.write(1, command);
}

AwaitableCoroutine SSD1309::enable() {
	//display::enableVcc(true);
	uint8_t command[1] = {0xAF};
	co_await this->spi.command.write(1, command);
	this->enabled = true;
}

AwaitableCoroutine SSD1309::disable() {
	uint8_t command[1] = {0xAE};
	co_await this->spi.command.write(1, command);
	this->enabled = false;
	//display::enableVcc(false);
}

AwaitableCoroutine SSD1309::setContrast(uint8_t contrast) {
	uint8_t command[2] = {0x81, contrast};
	co_await this->spi.command.write(2, command);
}
