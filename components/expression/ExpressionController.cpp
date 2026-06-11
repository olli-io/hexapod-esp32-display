#include "ExpressionController.h"

#include "Config.h"
#include "IRenderer.h"

namespace {
constexpr const char* kExprNames[static_cast<size_t>(Expression::_COUNT)] = {
    "NEUTRAL", "HAPPY", "SLEEPY", "SURPRISED", "ANGRY", "SAD", "BLINK",
};
constexpr const char* kGazeNames[static_cast<size_t>(GazeDirection::_COUNT)] = {
    "CENTER", "UP", "DOWN", "LEFT", "RIGHT",
    "UP_LEFT", "UP_RIGHT", "DOWN_LEFT", "DOWN_RIGHT",
};
}  // namespace

const char* expressionName(Expression e) {
    return isValid(e) ? kExprNames[static_cast<size_t>(e)] : "?";
}
const char* gazeName(GazeDirection g) {
    return isValid(g) ? kGazeNames[static_cast<size_t>(g)] : "?";
}

ExpressionController::ExpressionController(Display& display, IRenderer& renderer)
    : _display(display), _renderer(renderer) {}

bool ExpressionController::setExpression(Expression e) {
    if (!isValid(e)) return false;
    if (_expr != e) {
        _expr = e;
        _dirty = true;
    }
    return true;
}

bool ExpressionController::setGaze(GazeDirection g) {
    if (!isValid(g)) return false;
    if (_gaze != g) {
        _gaze = g;
        _dirty = true;
    }
    return true;
}

void ExpressionController::tick(uint32_t nowMs) {
    if (!_dirty) return;
    if (nowMs - _lastRenderMs < cfg::MIN_RENDER_INTERVAL_MS) return;

    RenderState s{_expr, _gaze};
    _renderer.render(_display, s);

    _dirty = false;
    _lastRenderMs = nowMs;
}
