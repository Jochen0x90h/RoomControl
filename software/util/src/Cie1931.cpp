#include "Cie1931.hpp"


Cie1931 hueToCie(float hue, float saturation) {
	float red, green, blue;

	// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
	float hh = hue;
	//if (hh >= 360.0) hh = 0.0;
	hh /= 60.0f;
	int i = int(hh);
	float ff = hh - i;
	float p = 1.0f - saturation;
	float q = 1.0f - saturation * ff;
	float t = 1.0f - saturation * (1.0f - ff);

	switch(i) {
	case 0:
		red = 1.0f;
		green = t;
		blue = p;
		break;
	case 1:
		red = q;
		green = 1.0f;
		blue = p;
		break;
	case 2:
		red = p;
		green = 1.0f;
		blue = t;
		break;
	case 3:
		red = p;
		green = q;
		blue = 1.0f;
		break;
	case 4:
		red = t;
		green = p;
		blue = 1.0f;
		break;
	default:
		red = 1.0f;
		green = p;
		blue = q;
		break;
	}

	// https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d
	float X = red * 0.649926f + green * 0.103455f + blue * 0.197109f;
	float Y = red * 0.234327f + green * 0.743075f + blue * 0.022598f;
	float Z = red * 0.0000000f + green * 0.053077f + blue * 1.035763f;

	float x = X / (X + Y + Z);
	float y = Y / (X + Y + Z);

	return {x, y};
}
