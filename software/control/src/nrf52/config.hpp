#pragma once

// flash
// -----

// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fnvmc.html&cp=4_0_0_3_2
static const int FLASH_PAGE_SIZE = 4096;
static const int FLASH_PAGE_COUNT = 16;
static const int FLASH_WRITE_ALIGN = 4;

// pins
// ----
constexpr int PORT0 = 0;
constexpr int PORT1 = 32;
constexpr int Disconnected = 0xffffffff;

// reset
constexpr int RESET_PIN = PORT0 | 26;

// digital potentiometer
#define HAVE_POTI
constexpr int POTI_A_PIN = PORT0 | 13;
constexpr int POTI_B_PIN = PORT0 | 24;
constexpr int BUTTON_PIN = PORT0 | 18;

// spi: display, air sensor, fe-ram
#define HAVE_SPI
constexpr int SPI_SCK_PIN = PORT0 | 2;
constexpr int SPI_MOSI_PIN = PORT1 | 13;
constexpr int SPI_MISO_PIN = PORT0 | 29; // also connected to display D/C#
constexpr int DISPLAY_CS_PIN = PORT0 | 31;
constexpr int AIR_SENSOR_CS_PIN = PORT0 | 3;
constexpr int FE_RAM_CS_PIN = PORT0 | 8;

// audio
#define HAVE_AUDIO
constexpr int AUDIO_EN_PIN = PORT1 | 0;
constexpr int AUDIO_BCLK_PIN = PORT1 | 4;
constexpr int AUDIO_LRCLK_PIN = PORT1 | 6;
constexpr int AUDIO_DIN_PIN = PORT1 | 2;

// motion detector
constexpr int MOTION_DETECTOR_COMP_PIN = PORT0 | 4;
constexpr int MOTION_DETECTOR_DS_PIN = PORT0 | 12; // delta-sigma

// brightness sensor
constexpr int BRIGHTNESS_SENSOR_PIN = PORT0 | 5;

// heating
constexpr int HEATING_PIN = PORT0 | 7;

// ad input
constexpr int AD_PIN = PORT0 | 30;

// bus
#define HAVE_BUS
constexpr int BUS_EN_PIN = PORT0 | 6;
constexpr int BUS_TX_PIN = PORT0 | 8;
constexpr int BUS_RX_PIN = PORT0 | 22;

// ext pins
constexpr int EXT_PIN1_PIN = PORT0 | 20;
constexpr int EXT_PIN2_PIN = PORT0 | 17;
constexpr int EXT_PIN3_PIN = PORT0 | 15;
