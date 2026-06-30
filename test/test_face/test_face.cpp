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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_expression_case_insensitive);
    RUN_TEST(test_parse_gaze_case_insensitive);
    RUN_TEST(test_parse_rejects_unknown);
    RUN_TEST(test_present_flushes_only_on_change);
    RUN_TEST(test_eyeanim_renders_into_panel);
    return UNITY_END();
}
