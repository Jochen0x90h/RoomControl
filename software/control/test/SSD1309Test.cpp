#include <SSD1309.hpp>
#include <Timer.hpp>
#include <QuadratureDecoder.hpp>
#include <Debug.hpp>
#include <Loop.hpp>


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
		co_await Timer::sleep(200ms);
		
		Debug::toggleRedLed();
	};
}


int main(void) {
	Loop::init();
	Timer::init();
	Spi::init();
	QuadratureDecoder::init();
	Output::init();
	
	draw();
	
	Loop::run();
}
