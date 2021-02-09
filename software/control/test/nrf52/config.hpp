#pragma once

// timer
// -----

constexpr int TIMER_COUNT = 4;


// pins for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/
// --------------------------------------------------------------------------------------

constexpr int PORT0 = 0;
constexpr int PORT1 = 32;
constexpr int Disconnected = 0xffffffff;

// quadrature decoder
constexpr int POTI_A_PIN = PORT0 | 4;
constexpr int POTI_B_PIN = PORT0 | 5;
constexpr int BUTTON_PIN = PORT0 | 6;

// debug led's
constexpr int LED_RED_PIN = PORT0 | 23;
constexpr int LED_GREEN_PIN = PORT0 | 22;
constexpr int LED_BLUE_PIN = PORT0 | 24;
