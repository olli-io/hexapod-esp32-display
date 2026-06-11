#include "u8g2_esp32_hal.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static u8g2_esp32_hal_config_t s_cfg = {
    .pin_sck = -1, .pin_mosi = -1, .pin_cs = -1, .pin_dc = -1, .pin_rst = -1,
};
static spi_device_handle_t s_spi;

void u8g2_esp32_hal_init(const u8g2_esp32_hal_config_t* config) {
    s_cfg = *config;
}

uint8_t u8g2_esp32_spi_byte_cb(u8x8_t* u8x8, uint8_t msg,
                               uint8_t arg_int, void* arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_INIT: {
            spi_bus_config_t bus = {
                .mosi_io_num = s_cfg.pin_mosi,
                .miso_io_num = -1,
                .sclk_io_num = s_cfg.pin_sck,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
            };
            ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
            spi_device_interface_config_t dev = {
                .mode = 0,
                .clock_speed_hz = (int)s_cfg.clock_hz,
                .spics_io_num = -1,
                .queue_size = 8,
            };
            ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &s_spi));
            break;
        }

        case U8X8_MSG_BYTE_SEND: {
            if (arg_int == 0) break;
            // u8g2 sends in chunks well under the DMA transfer limit, and
            // its buffers live in DRAM, so a polling transmit is safe here.
            spi_transaction_t t = {
                .length = (size_t)arg_int * 8,
                .tx_buffer = arg_ptr,
            };
            ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
            break;
        }

        case U8X8_MSG_BYTE_SET_DC:
            gpio_set_level(s_cfg.pin_dc, arg_int);
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
            break;

        case U8X8_MSG_BYTE_END_TRANSFER:
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
            break;

        default:
            return 0;
    }
    return 1;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t* u8x8, uint8_t msg,
                                     uint8_t arg_int, void* arg_ptr) {
    (void)u8x8;
    (void)arg_ptr;
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT: {
            uint64_t mask = 0;
            if (s_cfg.pin_cs >= 0)  mask |= 1ULL << s_cfg.pin_cs;
            if (s_cfg.pin_dc >= 0)  mask |= 1ULL << s_cfg.pin_dc;
            if (s_cfg.pin_rst >= 0) mask |= 1ULL << s_cfg.pin_rst;
            gpio_config_t io = {
                .pin_bit_mask = mask,
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            ESP_ERROR_CHECK(gpio_config(&io));
            // CS high before any SPI traffic so the SH1122 does not latch a
            // half-received command while the bus comes up.
            if (s_cfg.pin_cs >= 0) gpio_set_level(s_cfg.pin_cs, 1);
            break;
        }

        case U8X8_MSG_GPIO_CS:
            if (s_cfg.pin_cs >= 0) gpio_set_level(s_cfg.pin_cs, arg_int);
            break;

        case U8X8_MSG_GPIO_DC:
            if (s_cfg.pin_dc >= 0) gpio_set_level(s_cfg.pin_dc, arg_int);
            break;

        case U8X8_MSG_GPIO_RESET:
            if (s_cfg.pin_rst >= 0) gpio_set_level(s_cfg.pin_rst, arg_int);
            break;

        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int > 0 ? arg_int : 1));
            break;

        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(10);
            break;

        case U8X8_MSG_DELAY_100NANO:
        case U8X8_MSG_DELAY_NANO:
            esp_rom_delay_us(1);
            break;

        default:
            return 0;
    }
    return 1;
}
