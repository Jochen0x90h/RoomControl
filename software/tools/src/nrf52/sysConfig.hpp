#pragma once

// note: pins defined in this file are for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/

constexpr int PORT0 = 0;
constexpr int PORT1 = 32;


// out
// ---

constexpr struct {int pin; bool on;} OUT_CONFIGS[3] = {
	{PORT0 | 23, false},
	{PORT0 | 22, false},
	{PORT0 | 24, false}
};


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 2;
constexpr int RADIO_RECEIVE_QUEUE_LENGTH = 4;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// usb
// ---

constexpr int USB_ENDPOINT_COUNT = 3;
