#include "UartTransport.h"

UartTransport::UartTransport() = default;

bool UartTransport::begin(uint32_t baud, int rxPin, int txPin) {
    _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
    // Bump the driver's RX buffer above the default 256B so a busy Pi
    // burst plus a slow consumer (a render in the loop task) doesn't
    // overrun.
    _serial.setRxBufferSize(1024);
    return true;
}

size_t UartTransport::send(const uint8_t* data, size_t len) {
    return _serial.write(data, len);
}

void UartTransport::pump(const ByteSink& sink) {
    while (_serial.available() > 0) {
        const int b = _serial.read();
        if (b < 0) break;
        sink(static_cast<uint8_t>(b));
    }
}

void UartTransport::startRxTask(ByteSink sink,
                                uint32_t stackBytes,
                                UBaseType_t priority) {
    _sink = std::move(sink);
    xTaskCreate(rxTaskTrampoline,
                "uart_rx",
                stackBytes,
                this,
                priority,
                &_rxTask);
}

void UartTransport::rxTaskTrampoline(void* arg) {
    auto* self = static_cast<UartTransport*>(arg);
    for (;;) {
        self->pump(self->_sink);
        // Yield. 1 ms is plenty: at 921600 baud we receive ~92 B/ms and
        // Serial1's internal buffer is 1024 B, so the worst-case latency
        // before drain is ~11 ms of fill — well below overrun.
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
