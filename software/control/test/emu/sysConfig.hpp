#pragma once

// flash
// -----

// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fnvmc.html&cp=4_0_0_3_2
constexpr int FLASH_PAGE_SIZE = 4096;
constexpr int FLASH_PAGE_COUNT = 16;
constexpr int FLASH_WRITE_ALIGN = 4;


// out
// ---

constexpr int OUT_COLORS[4] = {0x0000ff, 0x00ff00, 0xff0000, 0xffffff};


// spi
// ---

constexpr int SPI_CONTEXT_COUNT = 2;


// display
// -------

constexpr int DISPLAY_QUEUE_LENGTH = 1;


// radio
// -----

constexpr int RADIO_CONTEXT_COUNT = 2;
constexpr int RADIO_MAX_PAYLOAD_LENGTH = 125; // payload length without leading length byte and trailing crc


// network
// -------

constexpr int NETWORK_CONTEXT_COUNT = 1;
