#pragma once

#include "IRenderer.h"

// Animated eye renderer. RenderState carries targets; this class owns the
// displayed state and the transitions between them:
//   - expression changes pass through a 260 ms blink, swapping at midpoint
//   - gaze eases to its target over 220 ms (easeOutCubic), composing with blinks
//   - idle blinks fire autonomously every 2.2–5.2 s
class EyeRenderer : public IRenderer {
public:
    void render(Display& display, const RenderState& state, uint32_t nowMs) override;
    void requestBlink() override { _blinkRequested = true; }

private:
    void  startBlink(uint32_t nowMs);
    float updateBlink(const RenderState& state, uint32_t nowMs);  // returns lid
    void  updateGaze(const RenderState& state, uint32_t nowMs);

    Expression    _shownExpr = Expression::NEUTRAL;
    GazeDirection _shownGaze = GazeDirection::CENTER;

    bool     _blinkActive    = false;
    bool     _swapDone       = false;  // midpoint expression swap fired
    bool     _blinkRequested = false;  // one-shot from TRIGGER_BLINK
    uint32_t _blinkStartMs   = 0;
    uint32_t _nextIdleBlinkMs = 0;

    float    _gx = 0, _gy = 0;          // current gaze offset, px
    float    _gazeFromX = 0, _gazeFromY = 0;
    float    _gazeToX = 0, _gazeToY = 0;
    uint32_t _gazeStartMs = 0;

    bool _started = false;              // arms the idle timer on first frame
};
