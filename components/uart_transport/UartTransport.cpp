#include "UartTransport.h"

bool UartTransport::begin(uint32_t baud, int rxPin, int txPin,
                          size_t rxRingSize, size_t txRingSize) {
    const uart_config_t conf = {
        .baud_rate = static_cast<int>(baud),
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {},
    };
    if (uart_driver_install(kPort, static_cast<int>(rxRingSize),
                            static_cast<int>(txRingSize),
                            0, nullptr, 0) != ESP_OK) {
        return false;
    }
    if (uart_param_config(kPort, &conf) != ESP_OK) return false;
    if (uart_set_pin(kPort, txPin, rxPin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        return false;
    }
    return true;
}

size_t UartTransport::send(const uint8_t* data, size_t len) {
    const int n = uart_write_bytes(kPort, data, len);
    return n < 0 ? 0 : static_cast<size_t>(n);
}

void UartTransport::pump(const ByteSink& sink) {
    uint8_t buf[64];
    for (;;) {
        const int n = uart_read_bytes(kPort, buf, sizeof(buf), 0);
        if (n <= 0) break;
        for (int i = 0; i < n; ++i) sink(buf[i]);
    }
}
