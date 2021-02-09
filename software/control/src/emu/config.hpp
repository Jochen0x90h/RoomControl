#pragma once

// flash
// -----

static const int FLASH_PAGE_SIZE = 4096;
static const int FLASH_PAGE_COUNT = 16;
static const int FLASH_WRITE_ALIGN = 4;

static const int TIMER_COUNT = 4;


// display
// -------

#define HAVE_SPI_DISPLAY
constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 64;
