#include <SSD1309.hpp>
#include <timer.hpp>
#include <display.hpp>
#include <poti.hpp>
#include <debug.hpp>
#include <loop.hpp>


Coroutine draw() {
	SSD1309 display;
	co_await display.init();
	co_await display.enable();
	
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> bitmap;
	int x = 0;
	int y = 0;
	while (true) {
		bitmap.clear();
		bitmap.drawRectangle(x, y, 10, 10);
		x = (x + 1) & (DISPLAY_WIDTH - 1);
		y = (y + 1) & (DISPLAY_HEIGHT - 1);
		
		co_await display.set(bitmap);
		co_await timer::sleep(200ms);
		
		debug::toggleRedLed();
	};
}


int main(void) {
	loop::init();
	timer::init();
	display::init();
	poti::init();
	gpio::init();
	
	draw();
	
	loop::run();
}
