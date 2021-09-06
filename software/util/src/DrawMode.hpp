#pragma once

#include "enum.hpp"


// draw modes
enum class DrawMode : uint8_t {
	// foreground
	FORE_CLEAR = 0,
	FORE_FLIP = 0x1,
	FORE_KEEP = 0x2,
	FORE_SET = 0x3,
	FORE_MASK = 0x3,
	
	// background
	BACK_CLEAR = 0x0,
	BACK_FLIP = 0x4,
	BACK_KEEP = 0x8,
	BACK_SET = 0xc,
	BACK_MASK = 0xc,
};
FLAGS_ENUM(DrawMode);
