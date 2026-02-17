// =====================================================
//  TFT_eSPI User_Setup.h for ESP32-2432S028 (CYD)
// =====================================================
//  COPY THIS FILE to replace:
//  C:\Users\User\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
// =====================================================

#define USER_SETUP_INFO "CYD ESP32-2432S028"

// ---- Display Driver ----
#define ILI9341_DRIVER

// ---- Display Size ----
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---- ESP32 Display Pins (HSPI) ----
#define USE_HSPI_PORT
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1  // Connected to EN/reset
#define TFT_BL   21  // Backlight

// ---- Touch ----
// NOTE: XPT2046 touch is on separate GPIO pins (not HSPI), handled in sketch code.
// Do NOT define TOUCH_CS here - it would try to use the wrong SPI bus.

// ---- SPI Frequency ----
#define SPI_FREQUENCY       27000000   // 27 MHz (safer for CYD clones)
#define SPI_READ_FREQUENCY  16000000

// ---- Misc ----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
