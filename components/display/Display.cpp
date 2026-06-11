#include "Display.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Config.h"
#include "u8g2_esp32_hal.h"

void Display::hardResetPanel() {
    // Configure RST here so the pulse works before u8g2's own GPIO init
    // (which happens later, inside u8g2_InitDisplay).
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << cfg::PIN_OLED_RST,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(static_cast<gpio_num_t>(cfg::PIN_OLED_RST), 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(static_cast<gpio_num_t>(cfg::PIN_OLED_RST), 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

bool Display::begin() {
    const u8g2_esp32_hal_config_t hal = {
        .pin_sck  = cfg::PIN_OLED_SCK,
        .pin_mosi = cfg::PIN_OLED_MOSI,
        .pin_cs   = cfg::PIN_OLED_CS,
        .pin_dc   = cfg::PIN_OLED_DC,
        .pin_rst  = cfg::PIN_OLED_RST,
        .clock_hz = cfg::OLED_SPI_HZ,
    };
    u8g2_esp32_hal_init(&hal);

    hardResetPanel();

    u8g2_Setup_sh1122_256x64_f(&_u8g2, U8G2_R0,
                               u8g2_esp32_spi_byte_cb,
                               u8g2_esp32_gpio_and_delay_cb);
    // Init order inside InitDisplay: GPIO init (CS driven high) -> SPI bus
    // init -> panel init sequence, so CS never floats during bus bring-up.
    u8g2_InitDisplay(&_u8g2);
    u8g2_SetPowerSave(&_u8g2, 0);
    u8g2_ClearBuffer(&_u8g2);
    return true;
}

void Display::clear() {
    u8g2_ClearBuffer(&_u8g2);
}

void Display::present() {
    u8g2_SendBuffer(&_u8g2);
}
