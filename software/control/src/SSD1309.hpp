#pragma once

#include <Bitmap.hpp>
#include <config.hpp>
#include <functional>


/**
 * Display controller for SSD1309
 */
class SSD1309 {
public:
	
	/**
	 * Constructor. Initializes the display and leaves it in disabled state.
	 * @param onInitialized called when the sensor is initialized and setParameters() needs to be called
	 */
	SSD1309(std::function<void ()> const &onInitialized);

	~SSD1309();

	/**
	 * Returns the number of send tasks that are in progress
	 */
	int getSendCount() {return this->sendCount;}

	/**
	 * Returns true if the display is enabled
	 */
	bool isEnabled() {return this->enabled;}
	
	/**
	 * Power up Vcc (if supported) and enable the display. Call after first onReady and Calls onReady when finished
	 */
	void enable();

	/**
	 * Disable the display and power down Vcc (if supported). Calls onReady when finished
	 */
	void disable();

	/**
	 * Set contrast. Calls onReady when finished
	 * @param contrast
	 */
	void setContrast(uint8_t contrast);

	/**
	 * Set whole display content. Calls onReady when finished
	 * @param bitmap bitmap to display
	 */
	void set(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> const &bitmap);

	/**
	 * Called when the display is ready for the next action
	 */
	std::function<void ()> onReady;

protected:

	void init1();
	void init2();
	void init3();
	void init4();
	void init5();
	void init6();
	void init7();
	void init8();
	void init9();
	void init10();
	void init11();
	void init12();
	void init13();

	uint8_t sendCount = 0;
	bool enabled = false;
	uint8_t command[4];
};
