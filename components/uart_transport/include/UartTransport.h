#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>

#include "driver/uart.h"

class UartTransport {
public:
    using ByteSink = std::function<void(uint8_t)>;

    UartTransport() = default;

    bool begin(uint32_t baud, int rxPin, int txPin,
               size_t rxRingSize = 2048, size_t txRingSize = 256);

    // Blocking write into the driver's TX ring, which is fast for the small
    // frames we send (tens to low hundreds of bytes). Returns bytes written.
    size_t send(const uint8_t* data, size_t len);

    // Drain whatever is available in the driver's RX ring through `sink`.
    // The driver ISR keeps filling the ring while nobody reads, so calling
    // this once per main-loop iteration is enough — no dedicated RX task,
    // which keeps codec and controller state single-task by construction.
    void pump(const ByteSink& sink);

private:
    static constexpr uart_port_t kPort = UART_NUM_1;
};
