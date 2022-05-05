#include "Menu.hpp"
#include <Timer.hpp>
#include <Poti.hpp>
#include <Input.hpp>
#include "tahoma_8pt.hpp" // font


Menu::Menu(SwapChain &swapChain) : swapChain(swapChain), bitmap(swapChain.get()) {}

void Menu::line() {
	int x = 10;
	int y = this->entryY + 2 - this->offsetY;
	if (this->bitmap != nullptr)
		this->bitmap->fillRectangle(x, y, 108, 1);
	this->entryY += 1 + 4;
}

void Menu::label() {
	/*int x = 10;
	int y = this->entryY + 2 - this->offsetY;
	if (this->bitmap != nullptr)
		this->bitmap->drawText(x, y, tahoma_8pt, s, 1);*/
	this->entryY += tahoma_8pt.height + 4;
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
	if (activated) {
		// return the bitmap to the swap chain without drawing it
		this->swapChain.put(this->bitmap);

		// trigger redraw
		this->bitmap = nullptr;
	}

	return activated;
}

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
	bool redraw = this->bitmap == nullptr;//this->redraw;
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
	this->delta = 0;
	this->activated = false;

	if (!redraw) {
		// show menu
		this->swapChain.show(this->bitmap);
		this->bitmap = nullptr;

		// get a new bitmap from the swap chain when poti::change returns or is interrupted
		BitmapGetter getter{*this};

		// wait for event, may be interrupted e.g. by a timeout
		int index;
		int s = co_await select(Poti::change(0, this->delta), Input::trigger(1 << INPUT_POTI_BUTTON, 0, index, this->activated));

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
	}
}
