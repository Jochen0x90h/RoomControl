#pragma once

#include <Bitmap.hpp>
#include <appConfig.hpp>
#include <Coroutine.hpp>


/**
 * Display controller for SSD1309
 */
class SSD1309 {
public:
	
	SSD1309() {}

	~SSD1309() {}

	/**
	 * Suspend execution using co_await until initialization is done
	 */
	AwaitableCoroutine init();

	/**
	 * Suspend execution using co_await until display is enabled.
	 * Powers down Vcc (if supported) and disables the display
	 */
	AwaitableCoroutine enable();

	/**
	 * Suspend execution using co_await until display is disabled
	 * Disable the display and power down Vcc (if supported). Calls onReady when finished
	 */
	AwaitableCoroutine disable();

	/**
	 * Returns true if the display is enabled
	 */
	bool isEnabled() {return this->enabled;}

	/**
	 * Suspend execution using co_await until contrast of display is set
	 *
	 */
	AwaitableCoroutine setContrast(uint8_t contrast);

	/**
	 * Suspend execution using co_await until until whole display content is set
	 * @param bitmap bitmap to display
	 */
	AwaitableCoroutine set(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> const &bitmap);

protected:

	bool enabled = false;
	uint8_t command[4];
};
