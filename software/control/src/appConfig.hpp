#pragma once


// timer
// -----

constexpr int TIMER_MQTTSN_CLIENT = 0;
constexpr int TIMER_MQTTSN_BROKER = 1;
constexpr int TIMER_LOCAL_INTERFACE = 2;
constexpr int TIMER_ROOM_CONTROL = 3;


// calendar
// --------

constexpr int CALENDAR_ROOM_CONTROL = 0;


// inputs
// ------

constexpr int INPUT_WHEEL_BUTTON = 0;
constexpr int INPUT_EXT_INDEX = 1;
constexpr int INPUT_EXT_COUNT = 1;


// outputs
// -------

constexpr int OUTPUT_DEBUG_RED = 0;
constexpr int OUTPUT_DEBUG_GREEN = 1;
constexpr int OUTPUT_DEBUG_BLUE = 2;
constexpr int OUTPUT_EXT_INDEX = 0;
constexpr int OUTPUT_EXT_COUNT = 3;
constexpr int OUTPUT_HEATING_INDEX = 3;
constexpr int OUTPUT_HEATING_COUNT = 1;


// spi
// ---

constexpr int SPI_AIR_SENSOR = 0;
constexpr int SPI_DISPLAY = 1;
constexpr int SPI_FRAM = 2;


// fram
// ----

constexpr int FRAM_SIZE = 8192;
constexpr int FRAM_WREN = 6; // write enable
constexpr int FRAM_WRDI = 4; // write disable
constexpr int FRAM_RDSR = 5; // read status register
constexpr int FRAM_WRSR = 1; // write status register
constexpr int FRAM_READ = 3; // read from memory array
constexpr int FRAM_WRITE = 2; // write to memory array
constexpr int FRAM_FSTRD = 11; // fast read from memory array
constexpr int FRAM_RDID = 0x9f; // read device id


// display
// -------

constexpr int DISPLAY_WIDTH = 128;
constexpr int DISPLAY_HEIGHT = 64;


// radio
// -----

constexpr int RADIO_ZBEE = 0;
constexpr int RADIO_MQTT = 1;


// network
// -------

constexpr int NETWORK_MQTT = 0;
constexpr int NETWORK_MDNS = 1;


// storage
// -------

constexpr int STORAGE_CONFIG = 0;
constexpr int STORAGE_ID_CONFIG = 0x0000;
constexpr int STORAGE_ID_DISPLAY_LISTENER = 0x0010;
constexpr int STORAGE_ID_BUS1 = 0x0100;
constexpr int STORAGE_ID_BUS2 = 0x0200;
constexpr int STORAGE_ID_RADIO1 = 0x0300;
constexpr int STORAGE_ID_RADIO2 = 0x0400;
constexpr int STORAGE_ID_ALARM = 0x0500;
constexpr int STORAGE_ID_FUNCTION = 0x0600;

constexpr int STORAGE_ID_CONNECTION = 0x1000;
