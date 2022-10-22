#pragma once

#include "SwapChain.hpp"
#include "tahoma_8pt.hpp" // font
#include <SystemTime.hpp>
#include <StringBuffer.hpp>
#include <StringOperators.hpp>


class Menu {
public:
	
	Menu(QuadratureDecoder &decoder, SwapChain &swapChain);

	/**
	 * Add a divider line to the menu
	 */
	void line();
	void beginSection();
	void endSection();

	class Stream : public ::Stream {
	public:
		int x;
		int y;
		Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap;
		int16_t underlineCount = 0;
		int16_t underlineStart;
		int16_t invertCount = 0;
		int16_t invertStart;


		Stream(int x, int y, Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap) : x(x), y(y), bitmap(bitmap) {}

		~Stream() override {}

		Stream &operator <<(char ch) override {
			if (this->bitmap != nullptr)
				this->x = this->bitmap->drawText(this->x, this->y, tahoma_8pt, String(1, &ch));
			return *this;
		}
		
		Stream &operator <<(String const &str) override {
			if (this->bitmap != nullptr)
				this->x = this->bitmap->drawText(this->x, this->y, tahoma_8pt, str);
			return *this;
		}
		
		Stream &operator <<(Command command) override {
			switch (command) {
				case Command::SET_UNDERLINE:
					if (this->underlineCount == 0)
						this->underlineStart = this->x;
					++this->underlineCount;
					break;
				case Command::CLEAR_UNDERLINE:
					if (this->underlineCount > 0) {
						if (--this->underlineCount == 0) {
							int x = this->underlineStart;
							if (this->bitmap != nullptr)
								this->bitmap->hLine(x, this->y + tahoma_8pt.height, this->x - x - 1);
						}
					}
					break;
				case Command::SET_INVERT:
					if (this->invertCount == 0)
						this->invertStart = this->x;
					++this->invertCount;
					break;
				case Command::CLEAR_INVERT:
					if (this->invertCount > 0) {
						if (--this->invertCount == 0) {
							int x = this->invertStart;
							if (this->bitmap != nullptr)
								this->bitmap->fillRectangle(x - 1, this->y, this->x - x + 1, tahoma_8pt.height, DrawMode::FLIP);
						}
					}
					break;
			}
			return *this;
		}
	};

	Stream stream();

	/**
	 * Add a label to the menu that can not be selected
 	 */
	void label();

	/**
	 * Add a label to the menu that can not be selected
	 * @param markup text with markup (e.g. underline)
	 */
	template <typename T>
	void label(T markup) {
		Stream s = stream();
		s << markup;
		label();
	}

	/**
	 * Add a menu entry
	 */
	bool entry();

	/**
	 * Add a menu entry
	 * @param markup text with markup (e.g. underline)
	 */
	template <typename T>
	bool entry(T markup) {
		Stream s = stream();
		s << markup;
		return entry();
	}

	int getSelected() const {return this->selected;}

	/**
	 * Returns true if the current entry is selected
	 */
	bool isSelected() const {
		return this->selected == this->entryIndex;
	}

	/**
	 * Get edit state. Returns 0 if not in edit mode or not the entry being edited, otherwise returns the 1-based index
	 * of the field being edited
	 */
	int getEdit(int editCount = 1);
	int getDelta() const {return this->delta;}

	void remove() {--this->selected;}

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

	QuadratureDecoder &decoder;
	SwapChain &swapChain;
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap;

	int8_t delta = 0;
	bool activated = false;

	enum class Section : uint8_t {
		BEGIN,
		BODY,
		END
	};
	Section section = Section::END;

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
