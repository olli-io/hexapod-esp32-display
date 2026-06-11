#include "Display.h"

#include <Arduino.h>
#include <SPI.h>

#include "Config.h"

Display::Display()
    : _u8g2(U8G2_R0,
            /*cs=*/  cfg::PIN_OLED_CS,
            /*dc=*/  cfg::PIN_OLED_DC,
            /*rst=*/ cfg::PIN_OLED_RST) {}

void Display::hardResetPanel() {
    pinMode(cfg::PIN_OLED_RST, OUTPUT);
    digitalWrite(cfg::PIN_OLED_RST, LOW);
    delay(10);
    digitalWrite(cfg::PIN_OLED_RST, HIGH);
    delay(50);
}

bool Display::begin() {
    // Drive CS high before any SPI traffic so SH1122 does not latch a
    // half-received command during SPI.begin().
    pinMode(cfg::PIN_OLED_CS, OUTPUT);
    digitalWrite(cfg::PIN_OLED_CS, HIGH);

    // The ESP32-C3 GPIO matrix means the default SPI pin set is whatever
    // the core was compiled with. Force our routing explicitly.
    // Signature: SPI.begin(sck, miso, mosi, ss). MISO=-1: SH1122 is
    // write-only over SPI.
    SPI.begin(cfg::PIN_OLED_SCK, -1, cfg::PIN_OLED_MOSI, cfg::PIN_OLED_CS);

    hardResetPanel();

    _u8g2.setBusClock(cfg::OLED_SPI_HZ);
    _u8g2.begin();
    _u8g2.clearBuffer();
    return true;
}

void Display::clear() {
    _u8g2.clearBuffer();
}

void Display::present() {
    _u8g2.sendBuffer();
}
