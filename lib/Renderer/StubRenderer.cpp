#include "StubRenderer.h"

#include <stdio.h>

#include "Config.h"
#include "Display.h"

void StubRenderer::render(Display& display, const RenderState& state) {
    display.clear();
    U8G2& g = display.u8g2();
    g.setFont(u8g2_font_6x10_tr);

    char line[40];

    snprintf(line, sizeof(line),
             "HEXAPOD DISPLAY v%u.%u.%u",
             cfg::FW_VER_MAJOR, cfg::FW_VER_MINOR, cfg::FW_VER_PATCH);
    g.drawStr(0, 12, line);

    snprintf(line, sizeof(line), "EXPR: %s", expressionName(state.expr));
    g.drawStr(0, 32, line);

    snprintf(line, sizeof(line), "GAZE: %s", gazeName(state.gaze));
    g.drawStr(0, 52, line);

    display.present();
}
