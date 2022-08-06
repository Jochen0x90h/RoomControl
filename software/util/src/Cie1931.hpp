#pragma once


/**
 * CIE 1931 xy color space
 */
struct Cie1931 {
	float x;
	float y;
};

/**
 * Convert hue and saturation to CIE 1931 xy color space
 * @param hue hue in the range [0, 360)
 * @param saturation saturation in the range [0, 1]
 * @return xy values
 */
Cie1931 hueToCie(float hue, float saturation);
