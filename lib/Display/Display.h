#pragma once

#include <U8g2lib.h>

class Display {
public:
    Display();

    // Initializes SPI with our explicit pins, hard-resets the SH1122,
    // brings up U8g2. Returns true on success.
    bool begin();

    void clear();    // clearBuffer only
    void present();  // sendBuffer — the single point of flush

    // Toggle the RST line: low 10 ms, high, wait 50 ms. Some SH1122 panels
    // wake into a wedged state on cold power-up; an explicit pulse fixes
    // the column-shift symptom.
    void hardResetPanel();

    // Escape hatch for renderers. They draw directly via U8g2 — no extra
    // canvas abstraction until a second display type exists.
    U8G2& u8g2() { return _u8g2; }

private:
    U8G2_SH1122_256X64_F_4W_HW_SPI _u8g2;
};
