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


// calendar
// --------

constexpr int CALENDAR_SECOND_HANDLER_COUNT = 1;


// spi
// ---

constexpr int AIR_SENSOR_CS_PIN = 0;
constexpr int FERAM_CS_PIN = 1;
