#include "EyeRenderer.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "Display.h"
#include "EyeRaster.h"

namespace {

const char* TAG = "eyes";

void pixelSink(void* ctx, int x, int y) {
    u8g2_DrawPixel(static_cast<u8g2_t*>(ctx),
                   static_cast<u8g2_uint_t>(x), static_cast<u8g2_uint_t>(y));
}

}  // namespace

void EyeRenderer::render(Display& display, const RenderState& state, uint32_t nowMs) {
    const eyes::AnimFrame f = _anim.update(state, nowMs);

    const int64_t t0 = esp_timer_get_time();

    display.clear();
    u8g2_t* g = &display.u8g2();
    eyes::drawEye(f.expr, false, eyes::kEyeLX, f.lid, f.gx, f.gy, pixelSink, g);
    eyes::drawEye(f.expr, true,  eyes::kEyeRX, f.lid, f.gx, f.gy, pixelSink, g);

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
