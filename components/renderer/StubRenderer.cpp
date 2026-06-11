#include "StubRenderer.h"

#include <stdio.h>

#include "Config.h"
#include "Display.h"

void StubRenderer::render(Display& display, const RenderState& state, uint32_t) {
    display.clear();
    u8g2_t* g = &display.u8g2();
    u8g2_SetFont(g, u8g2_font_6x10_tr);

    char line[40];

    snprintf(line, sizeof(line),
             "HEXAPOD DISPLAY v%u.%u.%u",
             cfg::FW_VER_MAJOR, cfg::FW_VER_MINOR, cfg::FW_VER_PATCH);
    u8g2_DrawStr(g, 0, 12, line);

    snprintf(line, sizeof(line), "EXPR: %s", expressionName(state.expr));
    u8g2_DrawStr(g, 0, 32, line);

    snprintf(line, sizeof(line), "GAZE: %s", gazeName(state.gaze));
    u8g2_DrawStr(g, 0, 52, line);

    display.present();
}
