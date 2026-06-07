// TFT_eSPI User Setup for VIEWE UEDX24240013-MD50E (ESP32-C3 + GC9A01)
// 240x240 1.3inch Round Display with Rotary Encoder

#define USER_SETUP_INFO "VIEWE UEDX24240013-MD50E GC9A01"

// Driver
#define GC9A01_DRIVER

// Display resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// SPI pins for ESP32-C3
#define TFT_CS   10     // Chip select control pin
#define TFT_DC    4     // Data Command control pin
#define TFT_RST  -1     // Set to -1 if display reset is not used
#define TFT_MOSI  0     // SPI MOSI pin
#define TFT_SCLK  1     // SPI SCLK pin
#define TFT_MISO -1     // Not used

// SPI frequency
#define SPI_FREQUENCY       80000000
#define SPI_READ_FREQUENCY  20000000

// Color depth - BGR order to match vendor MADCTL 0x48 (MX|BGR)
#define TFT_RGB_ORDER TFT_BGR

// Misc
#define SUPPORT_TRANSACTIONS