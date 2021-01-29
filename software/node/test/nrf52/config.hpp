#pragma once

constexpr int SYSTEM_TIMER_COUNT = 4;

// config for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/
constexpr int PORT0 = 0;
constexpr int PORT1 = 32;
constexpr int Disconnected = 0xffffffff;

// bus
constexpr int BUS_TX_PIN = PORT0 | 3;
constexpr int BUS_RX_PIN = PORT0 | 2;

// debug led's
constexpr int LED_RED_PIN = PORT0 | 23;
constexpr int LED_GREEN_PIN = PORT0 | 22;
constexpr int LED_BLUE_PIN = PORT0 | 24;
