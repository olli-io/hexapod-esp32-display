#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class UartTransport {
public:
    using ByteSink = std::function<void(uint8_t)>;

    UartTransport();

    bool begin(uint32_t baud, int rxPin, int txPin);

    // Blocking write. HardwareSerial maintains its own TX buffer so this is
    // fast for the small frames we send (tens to low hundreds of bytes).
    // Returns bytes actually written.
    size_t send(const uint8_t* data, size_t len);

    // Drain whatever is available in Serial1's RX buffer through `sink`.
    // Call from a loop or from the dedicated RX task.
    void pump(const ByteSink& sink);

    // Spawn a small FreeRTOS task that calls pump(sink) in a tight loop.
    // Priority 5 sits above Arduino's loopTask (prio 1) so UART RX is
    // never starved by U8g2 SPI bursts in the loop task.
    void startRxTask(ByteSink sink,
                     uint32_t stackBytes = 3072,
                     UBaseType_t priority = 5);

private:
    static void rxTaskTrampoline(void* arg);

    HardwareSerial _serial{1};   // UART1
    ByteSink       _sink;
    TaskHandle_t   _rxTask = nullptr;
};
