#pragma once

// note: pins defined in this file are for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/

constexpr int PORT0 = 0;
constexpr int PORT1 = 32;


// timer
// -----

constexpr int TIMER_COUNT = 4;


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 2;
constexpr int RADIO_RECEIVE_QUEUE_LENGTH = 2;
constexpr int RADIO_SEND_QUEUE_LENGTH = 2;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// usb
// ---

constexpr int USB_ENDPOINT_COUNT = 3;


// debug
// -----

constexpr int LED_RED_PIN = PORT0 | 23;
constexpr int LED_GREEN_PIN = PORT0 | 22;
constexpr int LED_BLUE_PIN = PORT0 | 24;
