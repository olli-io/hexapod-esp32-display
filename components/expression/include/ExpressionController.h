#pragma once

#include <stdint.h>

#include "Expression.h"

class Display;
class IRenderer;

class ExpressionController {
public:
    ExpressionController(Display& display, IRenderer& renderer);

    bool setExpression(Expression e);   // false if e out of range
    bool setGaze(GazeDirection g);

    Expression    expression() const { return _expr; }
    GazeDirection gaze()       const { return _gaze; }

    // Forward a one-shot blink request to the renderer.
    void triggerBlink();

    // Called from the loop. Renders every frame (animations run inside the
    // renderer); rate-limited to ~60 Hz.
    void tick(uint32_t nowMs);

private:
    Display&      _display;
    IRenderer&    _renderer;
    Expression    _expr = Expression::NEUTRAL;
    GazeDirection _gaze = GazeDirection::CENTER;
    uint32_t      _lastRenderMs = 0;
};
