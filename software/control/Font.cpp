#include "Font.hpp"

int Font::calcWidth(String text, int space) {
	int width = 0;
	for (unsigned char ch : text) {
		if (ch == '\t') {
			width += TAB_WIDTH;
		} else {
			if (ch < this->first || ch > this->last)
				ch = '?';

			// width of character
			width += this->characters[ch - this->first].width;
			
			// width of space
			width += space;
		}
	}
	return width;
}
