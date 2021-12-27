#pragma once

#include <Bitmap.hpp>
#include <appConfig.hpp>
#include <Coroutine.hpp>
#include <display.hpp>
#include <sys.hpp>


/**
 * Display controller for SSD1309
 */
class SSD1309 {
public:
	
	/**
	 * Initializate the display
	 * @return use co_await on return value to await end of initialization
	 */
	[[nodiscard]] AwaitableCoroutine init();

	/**
	 * Power up Vcc (if supported) and enable the display
	 * @return use co_await on return value to await end of operation
	 */
	[[nodiscard]] AwaitableCoroutine enable();

	/**
	 * Disable the display and power down Vcc (if supported)
	 * @return use co_await on return value to await end of operation
	 */
	[[nodiscard]] AwaitableCoroutine disable();

	/**
	 * Returns true if the display is enabled
	 */
	bool isEnabled() {return this->enabled;}

	/**
	 * Set contrast of display
	 * @return use co_await on return value to await end of operation
	 */
	[[nodiscard]] AwaitableCoroutine setContrast(uint8_t contrast);

	/**
	 * Set content of whole display
	 * @param bitmap bitmap to display
	 * @return use co_await on return value to await end of operation
	 */
	auto set(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> const &bitmap) {
		return display::send(0, array::count(bitmap.data), bitmap.data);
	}

protected:

	bool enabled = false;
	uint8_t command[4];
};
