#include "SSD1309.hpp"
#include <display.hpp>


SSD1309::SSD1309(std::function<void ()> onReady)
	: onReady(onReady)
{
	// command unlock
	this->command[0] = 0xFD;
	this->command[1] = 0x12;
	display::send(this->command, 2, 0, [this]() {init1();});
	++this->sendCount;
}

SSD1309::~SSD1309() {
}

void SSD1309::enable() {
	display::enableVcc(true);
	this->command[0] = 0xAF;
	this->sendCount += display::send(this->command, 1, 0, [this]() {
		--this->sendCount;
		this->enabled = true;
		this->onReady();
	});
}

void SSD1309::disable() {
	this->command[0] = 0xAE;
	this->sendCount += display::send(this->command, 1, 0, [this]() {
		--this->sendCount;
		this->enabled = false;
		this->onReady();
	});
	display::enableVcc(false);
}

void SSD1309::setContrast(uint8_t contrast) {
	this->command[0] = 0x81;
	this->command[1] = contrast;
	this->sendCount += display::send(this->command, 2, 0, [this]() {
		--this->sendCount;
		this->onReady();
	});
}

void SSD1309::set(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> const &bitmap) {
	this->sendCount += display::send(bitmap.data, 0, array::size(bitmap.data), [this]() {
		--this->sendCount;
		this->onReady();
	});
}

void SSD1309::init1() {
	// set display off
	this->command[0] = 0xAE;
	display::send(this->command, 1, 0, [this]() {init2();});
}

void SSD1309::init2() {
	// set display clock divide ratio / oscillator frequency
	this->command[0] = 0xD5;
	this->command[1] = 0xA0;
	display::send(this->command, 2, 0, [this]() {init3();});
}

void SSD1309::init3() {
	// set multiplex ratio
	this->command[0] = 0xA8;
	this->command[1] = 0x3F;
	display::send(this->command, 2, 0, [this]() {init4();});
}

void SSD1309::init4() {
	// set display offset
	this->command[0] = 0xD3;
	this->command[1] = 0x00;
	display::send(this->command, 2, 0, [this]() {init5();});
}

void SSD1309::init5() {
	// set display start line
	this->command[0] = 0x40;
	display::send(this->command, 1, 0, [this]() {init6();});
}

void SSD1309::init6() {
	// set segment remap
	this->command[0] = 0xA1;
	display::send(this->command, 1, 0, [this]() {init7();});
}

void SSD1309::init7() {
	// set com output scan direction
	this->command[0] = 0xC8;
	display::send(this->command, 1, 0, [this]() {init8();});
}

void SSD1309::init8() {
	// set com pins hardware configuration
	this->command[0] = 0xDA;
	this->command[1] = 0x12;
	display::send(this->command, 2, 0, [this]() {init9();});
}

void SSD1309::init9() {
	// set current control
	this->command[0] = 0x81;
	this->command[1] = 0xDF;
	display::send(this->command, 2, 0, [this]() {init10();});
}

void SSD1309::init10() {
	// set pre-charge period
	this->command[0] = 0xD9;
	this->command[1] = 0x82;
	display::send(this->command, 2, 0, [this]() {init11();});
}

void SSD1309::init11() {
	// set vcomh deselect level
	this->command[0] = 0xDB;
	this->command[1] = 0x34;
	display::send(this->command, 2, 0, [this]() {init12();});
}

void SSD1309::init12() {
	// set entire display to normal
	this->command[0] = 0xA4;
	display::send(this->command, 1, 0, [this]() {init13();});
}

void SSD1309::init13() {
	// set inverse display to normal
	this->command[0] = 0xA6;
	display::send(this->command, 1, 0, [this]() {
		--this->sendCount;
		this->onReady();
	});
}
