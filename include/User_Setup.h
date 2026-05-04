#pragma once

// TFT_eSPI configuration matching the device schematic.
// TFT uses HSPI; SD card uses VSPI. No bus sharing.

#define ILI9341_DRIVER

#define TFT_WIDTH   320
#define TFT_HEIGHT  240

// HSPI port for TFT
#define TFT_SPI_PORT HSPI

// TFT control pins
#define TFT_CS     15   // Chip select
#define TFT_DC      2   // Data/Command (RS)
#define TFT_RST    -1   // Reset: tied to ESP32 reset (-1) or connect to a GPIO if wired
#define TFT_BL     27   // Backlight control (active high)

// HSPI data pins
#define TFT_MOSI   13   // TFT_SDI / TP_DIN
#define TFT_SCLK   14   // TFT_CLK / TP_CLK
#define TFT_MISO   12   // TFT_SDO / TP_OUT (optional, for reads)
#define TOUCH_CS   33

// Fonts (keep minimal to save flash; add as needed)
#define LOAD_GLCD   // 8‑pixel font
#define LOAD_FONT2  // 16‑pixel font
#define LOAD_FONT4  // 26‑pixel font
#define LOAD_FONT6  // 48‑pixel font

// Keep smooth font disabled; this project uses bitmap fonts only.
#ifdef SMOOTH_FONT
#undef SMOOTH_FONT
#endif

// HSPI clock frequency (40 MHz typical)
#define SPI_FREQUENCY  40000000
