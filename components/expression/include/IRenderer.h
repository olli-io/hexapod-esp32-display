#pragma once

#include "Expression.h"

class Display;

struct RenderState {
    Expression    expr;
    GazeDirection gaze;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(Display& display, const RenderState& state) = 0;
};
