#pragma once

#include "IRenderer.h"

// v0 placeholder. Draws three text lines so the whole pipeline
// (SPI -> U8g2 -> UART -> codec -> dispatch) can be smoke-tested
// before any real eye art exists.
class StubRenderer : public IRenderer {
public:
    void render(Display& display, const RenderState& state) override;
};
