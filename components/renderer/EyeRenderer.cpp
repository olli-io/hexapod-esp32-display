#include "EyeRenderer.h"

#include <math.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

#include "Display.h"
#include "EyeRaster.h"

namespace {

const char* TAG = "eyes";

// Timing, matching the JS prototype. Wall-clock driven, fps-independent.
constexpr uint32_t kBlinkMs    = 260;   // full blink: close + open
constexpr uint32_t kLookMs     = 220;   // gaze ease duration
constexpr uint32_t kIdleMinMs  = 2200;  // idle blink: min interval...
constexpr uint32_t kIdleRandMs = 3001;  // ...plus rand() % this
constexpr float    kLidShut    = 0.92f; // lid travels 1 → 1-kLidShut → 1

uint32_t nextIdleDelay() { return kIdleMinMs + esp_random() % kIdleRandMs; }

float easeOutCubic(float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

// GazeDirection → unit offsets, indexed by enum value.
struct Dir {
    int8_t dx, dy;
};
constexpr Dir kGazeDir[static_cast<size_t>(GazeDirection::_COUNT)] = {
    {0, 0},   // CENTER
    {0, -1},  // UP
    {0, 1},   // DOWN
    {-1, 0},  // LEFT
    {1, 0},   // RIGHT
    {-1, -1}, // UP_LEFT
    {1, -1},  // UP_RIGHT
    {-1, 1},  // DOWN_LEFT
    {1, 1},   // DOWN_RIGHT
};

void pixelSink(void* ctx, int x, int y) {
    u8g2_DrawPixel(static_cast<u8g2_t*>(ctx),
                   static_cast<u8g2_uint_t>(x), static_cast<u8g2_uint_t>(y));
}

}  // namespace

void EyeRenderer::startBlink(uint32_t nowMs) {
    _blinkActive  = true;
    _swapDone     = false;
    _blinkStartMs = nowMs;
}

float EyeRenderer::updateBlink(const RenderState& state, uint32_t nowMs) {
    if (!_started) {
        _started         = true;
        _nextIdleBlinkMs = nowMs + nextIdleDelay();
    }

    if (!_blinkActive) {
        // A target change after the previous blink's midpoint chains a new
        // blink here; idle and commanded blinks swap nothing.
        if (state.expr != _shownExpr || _blinkRequested ||
            nowMs >= _nextIdleBlinkMs) {
            _blinkRequested = false;
            startBlink(nowMs);
        }
    }
    if (!_blinkActive) return 1.0f;
    _blinkRequested = false;  // already blinking; absorb the request

    const float p = static_cast<float>(nowMs - _blinkStartMs) / kBlinkMs;
    if (p >= 1.0f) {
        if (!_swapDone) _shownExpr = state.expr;  // midpoint frame was skipped
        _blinkActive     = false;
        _nextIdleBlinkMs = nowMs + nextIdleDelay();
        return 1.0f;
    }
    if (p >= 0.5f && !_swapDone) {
        _shownExpr = state.expr;
        _swapDone  = true;
    }
    const float half = p < 0.5f ? p / 0.5f : (1.0f - p) / 0.5f;  // 0 → 1 → 0
    return 1.0f - kLidShut * half;
}

void EyeRenderer::updateGaze(const RenderState& state, uint32_t nowMs) {
    if (state.gaze != _shownGaze) {
        _shownGaze   = state.gaze;
        const Dir d  = kGazeDir[static_cast<size_t>(state.gaze)];
        _gazeFromX   = _gx;
        _gazeFromY   = _gy;
        _gazeToX     = static_cast<float>(d.dx * eyes::kGazeMaxX);
        _gazeToY     = static_cast<float>(d.dy * eyes::kGazeMaxY);
        _gazeStartMs = nowMs;
    }
    const float t = fminf(1.0f, static_cast<float>(nowMs - _gazeStartMs) / kLookMs);
    const float e = easeOutCubic(t);
    _gx = _gazeFromX + (_gazeToX - _gazeFromX) * e;
    _gy = _gazeFromY + (_gazeToY - _gazeFromY) * e;
}

void EyeRenderer::render(Display& display, const RenderState& state, uint32_t nowMs) {
    const float lid = updateBlink(state, nowMs);
    updateGaze(state, nowMs);

    const int64_t t0 = esp_timer_get_time();

    display.clear();
    u8g2_t* g  = &display.u8g2();
    const int gx = static_cast<int>(lroundf(_gx));
    const int gy = static_cast<int>(lroundf(_gy));
    eyes::drawEye(_shownExpr, false, eyes::kEyeLX, lid, gx, gy, pixelSink, g);
    eyes::drawEye(_shownExpr, true,  eyes::kEyeRX, lid, gx, gy, pixelSink, g);

    const int64_t t1 = esp_timer_get_time();
    display.present();
    const int64_t t2 = esp_timer_get_time();

    // Soft-float budget check; visible with esp_log_level_set("eyes", DEBUG).
    static uint32_t lastLogMs = 0;
    if (nowMs - lastLogMs >= 1000) {
        lastLogMs = nowMs;
        ESP_LOGD(TAG, "raster %lld us, present %lld us",
                 static_cast<long long>(t1 - t0), static_cast<long long>(t2 - t1));
    }
}
