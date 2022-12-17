#pragma once

#include "../SpiMaster.hpp"
#include "Loop.hpp"


/**
 * Emulates a SSD1309 based display as an SPI slave device
 */
class SpiSSD1309 : public SpiMaster, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param width width of display
	 * @param height height of display
	 */
	SpiSSD1309(int width, int height);
	~SpiSSD1309() override;

	Awaitable <Parameters> transfer(int writeCount, void const *writeData, int readCount, void *readData) override;
	void transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) override;

	void handle(Gui &gui) override;

	// get display contents into an 8 bit grayscale image
	void getDisplay(uint8_t *buffer);


	int width;
	int height;

	// display layout: rows of 8 pixels where each byte describes a column in each row.
	// this would be the layout of a 16x16 display where each '|' is one byte:
	// ||||||||||||||||
	// ||||||||||||||||
	int column = 0; // column 0 to 127
	int page = 0; // page of 8 vertical pixels, 0 to 7
	int displayContrast = 255;
	bool displayOn = false; // all pixels on
	bool displayInverse = false;
	bool displayEnabled = false;
	uint8_t *data;
	uint8_t *displayBuffer;

	Waitlist<SpiMaster::Parameters> waitlist;


	/**
	 * Separate data channel
	 */
	class Data : public SpiMaster {
	public:
		/**
		 * Constructor
		 * @param device the SPI device to operate on
		 * @param csPin chip select pin of the slave
		 * @param writeOnly ture if we only write data and can use MISO as DC (data/command) signal (e.g. for a display)
		 */
		Data(SpiSSD1309 &display) : display(display) {}

		Awaitable<Parameters> transfer(int writeCount, const void *writeData, int readCount, void *readData) override;
		void transferBlocking(int writeCount, const void *writeData, int readCount, void *readData) override;


		SpiSSD1309 &display;
	};
};
