#pragma once

#include "Bitmap.hpp"


/**
 * Display interface
 */
class Display {
public:
	
	static int const WIDTH = 128;
	static int const HEIGHT = 64;
	
	Display() {}

	virtual ~Display();

	/**
	 * Set content to the display. Don't call again until onDisplayReady() gets called
	 * @param data display data
	 */
	void setDisplay(Bitmap<WIDTH, HEIGHT> const &bitmap);

	/**
	 * Gets called when setDisplay() finished and the display is ready again
	 */
	virtual void onDisplayReady() = 0;

	
	Bitmap<WIDTH, HEIGHT> emulatorDisplayBitmap;
};
