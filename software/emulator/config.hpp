#pragma once

// flash configuration
static const int FLASH_PAGE_SIZE = 1024;
static const int FLASH_PAGE_COUNT = 16;
static const int FLASH_WRITE_ALIGN = 2;

// maximum count for object types
static const int EVENT_COUNT = 32;
static const int TIMER_COUNT = 32;
static const int SCENARIO_COUNT = 32;
static const int DEVICE_COUNT = 32;

// inputs
static const unsigned int INPUT_COUNT = 1;
static const unsigned int INPUT_MOTION_DETECTOR = 0;

// outputs
static const unsigned int OUTPUT_COUNT = 9;
static const unsigned int OUTPUT_RELAY_1 = 0;
static const unsigned int OUTPUT_RELAY_2 = 1;
static const unsigned int OUTPUT_RELAY_3 = 2;
static const unsigned int OUTPUT_RELAY_4 = 3;
static const unsigned int OUTPUT_RELAY_5 = 4;
static const unsigned int OUTPUT_RELAY_6 = 5;
static const unsigned int OUTPUT_RELAY_7 = 6;
static const unsigned int OUTPUT_RELAY_8 = 7;
static const unsigned int OUTPUT_HEATING = 8;
