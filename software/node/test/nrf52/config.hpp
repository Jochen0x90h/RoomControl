#pragma once

// pins for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/
constexpr int PORT0 = 0;
constexpr int PORT1 = 32;
constexpr int Disconnected = 0xffffffff;


// flash
// -----

// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fnvmc.html&cp=4_0_0_3_2
constexpr int FLASH_PAGE_SIZE = 4096;
constexpr int FLASH_PAGE_COUNT = 16;
constexpr int FLASH_WRITE_ALIGN = 4;


// timer
// -----
constexpr int TIMER_COUNT = 4;


// spi
// ---

constexpr int SPI_TASK_COUNT = 4;
constexpr int SPI_SCK_PIN = PORT0 | 3;
constexpr int SPI_MOSI_PIN = PORT0 | 2;
constexpr int SPI_MISO_PIN = PORT0 | 21; // also connected to display D/C#
constexpr int SPI_CS_PIN = PORT0 | 19;
constexpr int SPI_CS_PINS[1] = {SPI_CS_PIN};


// display
// -------

#define HAVE_SPI_DISPLAY
constexpr int DISPLAY_CS_PIN = PORT0 | 20;
constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 64;


// bus
// ---

constexpr int BUS_TX_PIN = PORT0 | 3;
constexpr int BUS_RX_PIN = PORT0 | 2;


// debug led's
// -----------

constexpr int LED_RED_PIN = PORT0 | 23;
constexpr int LED_GREEN_PIN = PORT0 | 22;
constexpr int LED_BLUE_PIN = PORT0 | 24;
