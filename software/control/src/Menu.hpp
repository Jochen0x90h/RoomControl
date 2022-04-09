#pragma once

#include "SwapChain.hpp"
#include "tahoma_8pt.hpp" // font
#include <SystemTime.hpp>
#include <StringBuffer.hpp>
#include <StringOperators.hpp>


class Menu {
public:
	
	Menu(SwapChain &swapChain);
	
	/**
	 * Add a label to the menu that can not be selected
	 */
	void label(String s);

	/**
	 * Add a divider line to the menu
	 */
	void line();

	struct Stream {
		int x;
		int y;
		Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap;
		int16_t underlineCount = 0;
		int16_t underlineStart;
		
		Stream &operator <<(char ch) {
			if (this->bitmap != nullptr)
				this->x = this->bitmap->drawText(this->x, this->y, tahoma_8pt, String(1, &ch));
			return *this;
		}
		
		Stream &operator <<(String const &str) {
			if (this->bitmap != nullptr)
				this->x = this->bitmap->drawText(this->x, this->y, tahoma_8pt, str);
			return *this;
		}
		
		Stream &operator <<(StreamCommand command) {
			switch (command) {
			case StreamCommand::SET_UNDERLINE:
				if (this->underlineCount == 0)
					this->underlineStart = this->x;
				++this->underlineCount;
				break;
			case StreamCommand::CLEAR_UNDERLINE:
				if (this->underlineCount > 0) {
					if (--this->underlineCount == 0) {
						int s = this->underlineStart;
						if (this->bitmap != nullptr)
							this->bitmap->hLine(s, this->y + tahoma_8pt.height, this->x - s - 1);
					}
				}
				break;
			}
			return *this;
		}
	};

	Stream stream() {
		return {10, this->entryY + 2 - this->offsetY, this->bitmap};
	}

	bool entry();

	/**
	 * Add a menu entry
	 * @param markup graph of text with markup (e.g. underline)
	 */
	template <typename T>
	bool entry(T markup) {
		Stream s = stream();
		s << markup;
		return entry();
	}

	/**
	 * Add menu entry with weekday selector
	 */
	//bool entryWeekdays(String s, int weekdays, bool underline = false, int index = 0);

	/**
	 * Returns true if the current entry is selected
	 */
	bool isSelectedEntry() {
		return this->selected == this->entryIndex;
	}

	/**
	 * Get edit state. Returns 0 if not in edit mode or not the entry being edited, otherwise returns the 1-based index
	 * of the field being edited
	 */
	int getEdit(int editCount = 1);
	int getDelta() {return this->delta;}

	/**
	 * Show the menu on the display and wait for the next event
	 */
	AwaitableCoroutine show();

protected:

	struct BitmapGetter {
		Menu &menu;
		~BitmapGetter() {
			menu.bitmap = menu.swapChain.get();
		}
	};

	SwapChain &swapChain;
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap;

	int8_t delta = 0;
	bool activated = false;

	// index of selected menu entry
	uint16_t selected = 0;
	
	// y coordinate of selected menu entry
	uint16_t selectedY = 0;
		
	// starting y coodinate of display
	uint16_t offsetY = 0;


	// index of current menu entry
	uint16_t entryIndex = 0;
	
	// y coordinate of current menu entry
	uint16_t entryY = 0;

	// edit value of selected element
	uint16_t edit = 0;
};
