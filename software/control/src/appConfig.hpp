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


// in/out
// ------

constexpr int IN_COUNT = 0;
constexpr int OUT_COUNT = 4;


// spi
// ---

constexpr int SPI_AIR_SENSOR = 0;
constexpr int SPI_FRAM = 1;


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
