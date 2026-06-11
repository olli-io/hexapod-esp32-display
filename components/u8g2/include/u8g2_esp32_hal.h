#pragma once

#include <stdint.h>

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

// 4-wire hardware-SPI HAL for u8g2 on ESP-IDF (write-only bus, MISO unused).
// CS is driven manually as a GPIO via the u8x8 START/END_TRANSFER messages,
// not by the SPI peripheral, so it stays high across bus setup and idles
// exactly like the display expects.
typedef struct {
    int      pin_sck;
    int      pin_mosi;
    int      pin_cs;
    int      pin_dc;
    int      pin_rst;   // -1 if the reset line is managed outside u8g2
    uint32_t clock_hz;
} u8g2_esp32_hal_config_t;

// Store pin routing and clock. Call once before u8g2_InitDisplay(); the SPI
// bus and GPIOs are actually configured lazily via the u8x8 INIT messages.
void u8g2_esp32_hal_init(const u8g2_esp32_hal_config_t* config);

// Callbacks to pass to u8g2_Setup_*_4w_hw_spi-style setup functions.
uint8_t u8g2_esp32_spi_byte_cb(u8x8_t* u8x8, uint8_t msg,
                               uint8_t arg_int, void* arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t* u8x8, uint8_t msg,
                                     uint8_t arg_int, void* arg_ptr);

#ifdef __cplusplus
}
#endif
