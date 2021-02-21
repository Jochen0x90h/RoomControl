#pragma once


// flash
// -----

// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fnvmc.html&cp=4_0_0_3_2
constexpr int FLASH_PAGE_SIZE = 4096;
constexpr int FLASH_PAGE_COUNT = 16;
constexpr int FLASH_WRITE_ALIGN = 4;


// timer
// -----

constexpr int TIMER_COUNT = 4;
constexpr int CLIENT_TIMER_INDEX = 0;
constexpr int BROKER_TIMER_INDEX = 1;
constexpr int CONTROL_TIMER_INDEX = 2;
constexpr int LOCAL_DEVICES_TIMER_INDEX = 3;


// spi
// ---

constexpr int AIR_SENSOR_CS_PIN = 0;
constexpr int FERAM_CS_PIN = 1;


// display
// -------

constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 64;


// bus and radio devices
// ---------------------
constexpr int MAX_DEVICE_COUNT = 64;