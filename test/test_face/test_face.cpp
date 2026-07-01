#include <unity.h>

#include "EyeAnim.h"
#include "EyeRaster.h"
#include "FaceNames.h"
#include "Sh1122Panel.h"

void setUp() {}
void tearDown() {}

// --- name <-> enum parsing (the ROS topic vocabulary) -----------------------

void test_parse_expression_case_insensitive(void) {
    TEST_ASSERT_TRUE(face::parseExpression("HAPPY") == Expression::HAPPY);
    TEST_ASSERT_TRUE(face::parseExpression("happy") == Expression::HAPPY);
    TEST_ASSERT_TRUE(face::parseExpression("HaPpY") == Expression::HAPPY);
    TEST_ASSERT_TRUE(face::parseExpression("LOVE")  == Expression::LOVE);
}

void test_parse_gaze_case_insensitive(void) {
    TEST_ASSERT_TRUE(face::parseGaze("center")  == GazeDirection::CENTER);
    TEST_ASSERT_TRUE(face::parseGaze("up_left") == GazeDirection::UP_LEFT);
}

void test_parse_rejects_unknown(void) {
    TEST_ASSERT_FALSE(face::parseExpression("nope").has_value());
    TEST_ASSERT_FALSE(face::parseExpression("").has_value());
    TEST_ASSERT_FALSE(face::parseGaze("sideways").has_value());
}

// --- dirty-buffer flush (only push changed frames over SPI) -----------------

void test_present_flushes_only_on_change(void) {
    face::PanelConfig cfg;
    cfg.headless = true;  // exercises the full pipeline minus device I/O
    face::Sh1122Panel panel;
    TEST_ASSERT_TRUE(panel.begin(cfg));

    // First present after begin: blank buffer differs from "nothing flushed".
    TEST_ASSERT_TRUE(panel.present());
    TEST_ASSERT_EQUAL_UINT64(1, panel.flushCount());

    // Identical frame: no flush.
    TEST_ASSERT_FALSE(panel.present());
    TEST_ASSERT_EQUAL_UINT64(1, panel.flushCount());

    // Changed frame: flush.
    u8g2_DrawPixel(panel.u8g2(), 10, 10);
    TEST_ASSERT_TRUE(panel.present());
    TEST_ASSERT_EQUAL_UINT64(2, panel.flushCount());

    // Same again: no flush.
    TEST_ASSERT_FALSE(panel.present());
    TEST_ASSERT_EQUAL_UINT64(2, panel.flushCount());
}

// --- end-to-end: the reused animation core draws into the panel buffer ------

void test_eyeanim_renders_into_panel(void) {
    face::PanelConfig cfg;
    cfg.headless = true;
    face::Sh1122Panel panel;
    TEST_ASSERT_TRUE(panel.begin(cfg));

    auto rng = +[]() -> uint32_t { return 1u; };  // deterministic, no idle blink
    eyes::EyeAnim anim(rng);
    RenderState target{Expression::HAPPY, GazeDirection::LEFT};

    const eyes::AnimFrame f = anim.update(target, 0);
    panel.clearBuffer();
    eyes::drawEye(f.expr, false, eyes::kEyeLX, f.lid, f.gx, f.gy,
                  [](void* ctx, int x, int y) {
                      u8g2_DrawPixel(static_cast<u8g2_t*>(ctx), x, y);
                  },
                  panel.u8g2());
    TEST_ASSERT_TRUE(panel.present());  // non-blank frame must flush
    TEST_ASSERT_EQUAL_UINT64(1, panel.flushCount());
}

// --- raster-skip premise: EyeAnim is static when idle, moves during a blink --
// The node skips the raster when consecutive AnimFrames are identical; that only
// helps if idle frames really are bit-identical. Guard that assumption here.

static bool frameEq(const eyes::AnimFrame& a, const eyes::AnimFrame& b) {
    return a.expr == b.expr && a.lid == b.lid && a.gx == b.gx && a.gy == b.gy;
}

void test_idle_frames_are_static(void) {
    auto rng = +[]() -> uint32_t { return 0u; };  // idle blink at +2200 ms
    eyes::EyeAnim anim(rng);
    RenderState target{Expression::NEUTRAL, GazeDirection::CENTER};
    anim.update(target, 0);  // arm the idle timer

    const eyes::AnimFrame a = anim.update(target, 500);
    const eyes::AnimFrame b = anim.update(target, 1500);  // still < 2200: no blink
    TEST_ASSERT_TRUE(frameEq(a, b));    // identical -> raster is skipped
    TEST_ASSERT_EQUAL_FLOAT(1.0f, b.lid);
}

void test_blink_changes_frames(void) {
    auto rng = +[]() -> uint32_t { return 0u; };
    eyes::EyeAnim anim(rng);
    RenderState target{Expression::NEUTRAL, GazeDirection::CENTER};
    anim.update(target, 0);
    anim.requestBlink();

    const eyes::AnimFrame a = anim.update(target, 10);   // blink just started
    const eyes::AnimFrame b = anim.update(target, 80);   // mid-blink: lid closing
    TEST_ASSERT_FALSE(frameEq(a, b));   // differs -> raster runs
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_expression_case_insensitive);
    RUN_TEST(test_parse_gaze_case_insensitive);
    RUN_TEST(test_parse_rejects_unknown);
    RUN_TEST(test_present_flushes_only_on_change);
    RUN_TEST(test_eyeanim_renders_into_panel);
    RUN_TEST(test_idle_frames_are_static);
    RUN_TEST(test_blink_changes_frames);
    return UNITY_END();
}
