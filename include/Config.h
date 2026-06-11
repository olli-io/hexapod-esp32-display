#pragma once

#include <stdint.h>

// XIAO ESP32-C3 silkscreen aliases (D0..D10) are defined by the Arduino core
// as macros mapping to the underlying GPIO numbers. Keep this header
// portable by gating Arduino-specific bits.
#ifdef ARDUINO
  #include <Arduino.h>
#endif

namespace cfg {

// ---------------------------------------------------------------------------
// Firmware version (FW_VERSION_* come from -D flags in platformio.ini)
// ---------------------------------------------------------------------------
constexpr uint8_t FW_VER_MAJOR = FW_VERSION_MAJOR;
constexpr uint8_t FW_VER_MINOR = FW_VERSION_MINOR;
constexpr uint8_t FW_VER_PATCH = FW_VERSION_PATCH;

// ---------------------------------------------------------------------------
// Pin map — XIAO ESP32-C3
// ---------------------------------------------------------------------------
// SPI to SH1122 OLED (4-wire HW SPI, U8g2 _4W_HW_SPI constructor)
//   D8  GPIO8   SCK   (strapping; HIGH at boot. SPI mode 0 idles low after init.)
//   D10 GPIO10  MOSI
//   D3  GPIO5   CS
//   D2  GPIO4   DC
//   D1  GPIO3   RST
//
// UART1 to Raspberry Pi
//   D6  GPIO21  TX
//   D7  GPIO20  RX
//
// Reserved / left free: D0 (GPIO2 strapping), D4/D5 (I2C SDA/SCL),
// D9 (GPIO9 BOOT button).
#ifdef ARDUINO
constexpr int PIN_OLED_SCK  = D8;
constexpr int PIN_OLED_MOSI = D10;
constexpr int PIN_OLED_CS   = D3;
constexpr int PIN_OLED_DC   = D2;
constexpr int PIN_OLED_RST  = D1;

constexpr int PIN_UART_TX   = D6;
constexpr int PIN_UART_RX   = D7;
#endif

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
constexpr uint32_t OLED_SPI_HZ = 8'000'000;

// ---------------------------------------------------------------------------
// UART link to Pi
// ---------------------------------------------------------------------------
constexpr uint32_t UART_BAUD     = 921600;
constexpr size_t   RX_RING_SIZE  = 512;
constexpr size_t   TX_RING_SIZE  = 256;

// ---------------------------------------------------------------------------
// Render loop
// ---------------------------------------------------------------------------
constexpr uint32_t MIN_RENDER_INTERVAL_MS = 16;  // ~60 Hz cap

}  // namespace cfg
