#include "Menu.hpp"
#include <poti.hpp>
#include "tahoma_8pt.hpp" // font


constexpr String weekdaysShort[7] = {"M", "T", "W", "T", "F", "S", "S"};


void Menu::label(String s) {
	int x = 10;
	int y = this->entryY + 2 - this->offsetY;
	if (this->bitmap != nullptr)
		this->bitmap->drawText(x, y, tahoma_8pt, s, 1);
	this->entryY += tahoma_8pt.height + 4;
}

void Menu::line() {
	int x = 10;
	int y = this->entryY + 2 - this->offsetY;
	if (this->bitmap != nullptr)
		this->bitmap->fillRectangle(x, y, 108, 1);
	this->entryY += 1 + 4;
}
/*
bool Menu::entry(String s, bool underline, int begin, int end) {
	const int lineHeight = tahoma_8pt.height + 4;

	int x = 10;
	int y = this->entryY + 2 - this->offsetY;
	this->bitmap.drawText(x, y, tahoma_8pt, s);
	if (underline) {
		int start = tahoma_8pt.calcWidth(s.substring(0, begin));
		int width = tahoma_8pt.calcWidth(s.substring(begin, end)) - 1;
		this->bitmap.hLine(x + start, y + tahoma_8pt.height, width);
	}

	bool selected = this->entryIndex == this->selected;
	if (selected) {
		this->bitmap.drawText(0, y, tahoma_8pt, ">", 0);
		this->selectedY = this->entryY;
	}

	++this->entryIndex;
	this->entryY += lineHeight;

	return selected && this->activated;
}

bool Menu::entryWeekdays(String s, int weekdays, bool underline, int index) {
	int x = 10;
	int y = this->entryY + 2 - this->offsetY;

	// text (e.g. time)
	int x2 = this->bitmap.drawText(x, y, tahoma_8pt, s, 1) + 1;
	int start = x;
	int width = x2 - x;

	// week days
	for (int i = 0; i < 7; ++i) {
		int x3 = this->bitmap.drawText(x2 + 1, y, tahoma_8pt, weekdaysShort[i], 1);
		if (weekdays & 1)
			this->bitmap.fillRectangle(x2, y, x3 - x2, tahoma_8pt.height - 1, Mode::FLIP);
		if (i == index) {
			start = x2;
			width = x3 - x2;
		}
		x2 = x3 + 4;
		weekdays >>= 1;
	}

	bool selected = this->entryIndex == this->selected;
	if (selected) {
		this->bitmap.drawText(0, y, tahoma_8pt, ">", 1);
		this->selectedY = this->entryY;
	}
	
	if (underline) {
		this->bitmap.hLine(start, y + tahoma_8pt.height, width);
	}

	++this->entryIndex;
	this->entryY += tahoma_8pt.height + 4;

	return selected && this->activated;
}
*/
int Menu::getEdit(int editCount) {
	// check if the next entry is selected
	if (this->selected == this->entryIndex) {
		// cycle edit mode if activated
		if (this->activated) {
			if (this->edit < editCount)
				++this->edit;
			else
				this->edit = 0;
				
			// "consume" activation
			this->activated = false;
		}
		return this->edit;
	}
	return 0;
}

AwaitableCoroutine Menu::show() {
	const int lineHeight = tahoma_8pt.height + 4;

	// adjust yOffset so that selected entry is visible
	bool redraw = this->bitmap == nullptr;
	int upper = this->selectedY;
	int lower = upper + lineHeight;
	if (upper < this->offsetY) {
		this->offsetY = upper;
		redraw = true;
	}
	if (lower > this->offsetY + DISPLAY_HEIGHT) {
		this->offsetY = lower - DISPLAY_HEIGHT;
		redraw = true;
	}

	// number of entries in menu
	int entryCount = this->entryIndex;

	// clear for next menu drawing
	this->entryIndex = 0;
	this->entryY = 0;

	if (!redraw) {
		// show menu
		this->swapChain.show(this->bitmap);

		// get a new bitmap from the swap chain when poti::change returns or is interrupted
		BitmapGetter getter{*this};

		// wait for event, may be interrupted e.g. by a timeout
		co_await poti::change(this->delta, this->activated);

		// update selected according to delta motion of poti when not in edit mode
		if (this->edit == 0) {
			int selected = this->selected + this->delta;
			if (selected < 0) {
				selected = 0;

				// also clear yOffset in case the menu has a non-selectable header
				this->offsetY = 0;
			} else if (selected >= entryCount) {
				selected = entryCount - 1;
			}
			this->selected = selected;
		}
	} else {
		// redraw menu
		if (this->bitmap == nullptr)
			this->bitmap = this->swapChain.get();
		else
			this->bitmap->clear();
		this->delta = 0;
		this->activated = false;
	}
}

bool Menu::entry() {
	const int lineHeight = tahoma_8pt.height + 4;
	int y = this->entryY + 2 - this->offsetY;

	bool selected = this->entryIndex == this->selected;
	if (selected) {
		if (this->bitmap != nullptr)
			this->bitmap->drawText(0, y, tahoma_8pt, ">", 0);
		this->selectedY = this->entryY;
	}

	++this->entryIndex;
	this->entryY += lineHeight;

	// check if this menu entry was activated
	bool activated = selected && this->activated;

	// invalidate bitmap when the entry was selected
	if (activated) {
		this->swapChain.put(this->bitmap);
		this->bitmap = nullptr;
	}

	return activated;
}
