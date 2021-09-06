#pragma once

#include "SSD1309.hpp"


/**
 * A swap chain that manages bitmaps for showing on a display
 */
class SwapChain {
public:
	/**
	 * Constructor starts a coroutine that transfers the bitmaps to the display
	 */
	SwapChain() : freeList{&bitmaps[0], &bitmaps[1]} {
		// start transfer coroutine
		transfer();
	}
	
	/**
	 * Get a cleared bitmap from the swap chain
	 */
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *get();

	/**
	 * Put a bitmap back to the swap chain
	 */
	void put(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap);

	/**
	 * Add the bitmap to the show queue. A subsequent get() may remove it again when the transfer has not started yet.
	 */
	void show(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap);

protected:

	Coroutine transfer();

	// two bitmaps
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> bitmaps[2];

	int freeHead = 2;
	int freeTail = 0;
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *freeList[2];
	
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *showList = nullptr;
	
	SSD1309 display;
	Barrier<> barrier;
};
