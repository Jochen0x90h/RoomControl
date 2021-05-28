#pragma once

// note: pins defined in this file are for nRF52840 MDK USB Dongle, https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/

constexpr int PORT0 = 0;
constexpr int PORT1 = 32;


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

constexpr int SPI_TRANSFER_QUEUE_LENGTH = 4;
constexpr int SPI_SCK_PIN = PORT0 | 19;
constexpr int SPI_MOSI_PIN = PORT0 | 20;
constexpr int SPI_MISO_PIN = PORT0 | 21; // also connected to display D/C#
constexpr int SPI_CS_PINS[2] = {PORT0 | 2, PORT0 | 3};


// bus
// ---

constexpr int BUS_TX_PIN = PORT0 | 3;
constexpr int BUS_RX_PIN = PORT0 | 2;


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 2;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc
constexpr int RADIO_RECEIVE_QUEUE_LENGTH = 2;
constexpr int RADIO_SEND_QUEUE_LENGTH = 2;


// usb
// ---

constexpr int USB_ENDPOINT_COUNT = 3;


// poti
// ----
constexpr int POTI_A_PIN = PORT0 | 4;
constexpr int POTI_B_PIN = PORT0 | 5;
constexpr int BUTTON_PIN = PORT0 | 6;


// display
// -------

constexpr int DISPLAY_CS_PIN = PORT0 | 3;


// motion detector
// ---------------

//constexpr int MOTION_DETECTOR_DELTA_SIGMA_PIN = PORT0 | 3;
//constexpr int MOTION_DETECTOR_COMPARATOR_INPUT = 0; // analog input
//constexpr int MOTION_DETECTOR_COMPARATOR_PIN = PORT0 | 2;

constexpr int MOTION_DETECTOR_PIR_PIN = PORT0 | 2;
constexpr int MOTION_DETECTOR_PIR_INPUT = 0;
constexpr int MOTION_DETECTOR_TRACKER_PIN = PORT0 | 3;
constexpr int MOTION_DETECTOR_TRACKER_INPUT = 1;


// debug
// -----

constexpr int LED_RED_PIN = PORT0 | 23;
constexpr int LED_GREEN_PIN = PORT0 | 22;
constexpr int LED_BLUE_PIN = PORT0 | 24;
