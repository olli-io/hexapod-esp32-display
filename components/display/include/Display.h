#pragma once

#include "u8g2.h"

class Display {
public:
    Display() = default;

    // Registers the SPI bus with our explicit pins, hard-resets the SH1122,
    // brings up u8g2. Returns true on success.
    bool begin();

    void clear();    // ClearBuffer only
    void present();  // SendBuffer — the single point of flush

    // Toggle the RST line: low 10 ms, high, wait 50 ms. Some SH1122 panels
    // wake into a wedged state on cold power-up; an explicit pulse fixes
    // the column-shift symptom.
    void hardResetPanel();

    // Escape hatch for renderers. They draw directly via the u8g2 C API —
    // no extra canvas abstraction until a second display type exists.
    u8g2_t& u8g2() { return _u8g2; }

private:
    u8g2_t _u8g2{};
};
