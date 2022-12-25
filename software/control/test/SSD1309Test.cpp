#include <SSD1309.hpp>
#include <QuadratureDecoder.hpp>
#include <Debug.hpp>
#include <Loop.hpp>
#include <boardConfig.hpp>


Coroutine draw(SSD1309::Spi spi) {
	SSD1309 display(spi);
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
		co_await loop::sleep(200ms);
		
		debug::toggleRed();
	}
}


int main(void) {
	loop::init();
	Output::init();
	Drivers drivers;

	draw({drivers.displayCommand, drivers.displayData});

	loop::run();
}
