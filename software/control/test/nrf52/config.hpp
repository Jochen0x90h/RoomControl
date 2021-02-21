#pragma once

// pins for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/
constexpr int PORT0 = 0;
constexpr int PORT1 = 32;
constexpr int Disconnected = 0xffffffff;


// timer
// -----

constexpr int TIMER_COUNT = 4;


// poti
// ----
constexpr int POTI_A_PIN = PORT0 | 4;
constexpr int POTI_B_PIN = PORT0 | 5;
constexpr int BUTTON_PIN = PORT0 | 6;


// spi
// ---

constexpr int SPI_TASK_COUNT = 4;
constexpr int SPI_SCK_PIN = PORT0 | 3;
constexpr int SPI_MOSI_PIN = PORT0 | 2;
constexpr int SPI_MISO_PIN = PORT0 | 21; // also connected to display D/C#
constexpr int AIR_SENSOR_CS_PIN = PORT0 | 19;
constexpr int FERAM_CS_PIN = PORT0 | 19;
constexpr int SPI_CS_PINS[2] = {AIR_SENSOR_CS_PIN, FERAM_CS_PIN};


// display
// -------

#define HAVE_SPI_DISPLAY
constexpr int DISPLAY_CS_PIN = PORT0 | 20;
constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 64;


// debug
// -----

constexpr int LED_RED_PIN = PORT0 | 23;
constexpr int LED_GREEN_PIN = PORT0 | 22;
constexpr int LED_BLUE_PIN = PORT0 | 24;
