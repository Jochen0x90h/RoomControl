#pragma once

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

constexpr int RADIO_ZB = 0;


// network
// -------

constexpr int NETWORK_MQTTSN = 0;
