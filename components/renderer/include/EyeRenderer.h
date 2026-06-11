#pragma once

#include "esp_random.h"

#include "EyeAnim.h"
#include "IRenderer.h"

// Animated eye renderer: EyeAnim drives the blink/gaze/idle state machine,
// EyeRaster draws the result into the u8g2 buffer.
class EyeRenderer : public IRenderer {
public:
    void render(Display& display, const RenderState& state, uint32_t nowMs) override;
    void requestBlink() override { _anim.requestBlink(); }

private:
    eyes::EyeAnim _anim{esp_random};
};
