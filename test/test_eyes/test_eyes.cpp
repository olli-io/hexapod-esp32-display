#include <unity.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EyeRaster.h"

// Local name table: expressionName() lives in ExpressionController.cpp,
// which this suite intentionally does not link.
static const char* exprName(Expression e) {
    static const char* names[] = {"NEUTRAL", "HAPPY",  "SLEEPY", "DEAD",
                                  "GREEDY",  "WOOZY",  "ANGRY",  "LOVE"};
    return names[static_cast<size_t>(e)];
}

// 1-bit framebuffer test sink. The raster module clips before emitting, so
// every callback must already be in-bounds — verified in each test.
static uint8_t fb[eyes::kH][eyes::kW];
static bool    outOfBounds;

static void sink(void*, int x, int y) {
    if (x < 0 || x >= eyes::kW || y < 0 || y >= eyes::kH) {
        outOfBounds = true;
        return;
    }
    fb[y][x] = 1;
}

static void clearFb() {
    memset(fb, 0, sizeof(fb));
    outOfBounds = false;
}

static int litPixels() {
    int n = 0;
    for (int y = 0; y < eyes::kH; ++y)
        for (int x = 0; x < eyes::kW; ++x) n += fb[y][x];
    return n;
}

struct BBox {
    int minX = eyes::kW, maxX = -1, minY = eyes::kH, maxY = -1;
};
static BBox bbox() {
    BBox b;
    for (int y = 0; y < eyes::kH; ++y)
        for (int x = 0; x < eyes::kW; ++x)
            if (fb[y][x]) {
                if (x < b.minX) b.minX = x;
                if (x > b.maxX) b.maxX = x;
                if (y < b.minY) b.minY = y;
                if (y > b.maxY) b.maxY = y;
            }
    return b;
}

static void drawBoth(Expression e, float lid, int gx, int gy) {
    clearFb();
    eyes::drawEye(e, false, eyes::kEyeLX, lid, gx, gy, sink, nullptr);
    eyes::drawEye(e, true, eyes::kEyeRX, lid, gx, gy, sink, nullptr);
}

void setUp() {}
void tearDown() {}

void test_every_expression_draws_pixels(void) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Expression::_COUNT); ++i) {
        drawBoth(static_cast<Expression>(i), 1.0f, 0, 0);
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, litPixels(), exprName(static_cast<Expression>(i)));
        TEST_ASSERT_FALSE(outOfBounds);
    }
}

void test_extreme_gaze_and_lid_stay_in_bounds(void) {
    const int   gxs[]  = {-eyes::kGazeMaxX, 0, eyes::kGazeMaxX};
    const int   gys[]  = {-eyes::kGazeMaxY, 0, eyes::kGazeMaxY};
    const float lids[] = {0.08f, 0.5f, 1.0f};
    for (uint8_t i = 0; i < static_cast<uint8_t>(Expression::_COUNT); ++i)
        for (int gx : gxs)
            for (int gy : gys)
                for (float lid : lids) {
                    drawBoth(static_cast<Expression>(i), lid, gx, gy);
                    TEST_ASSERT_FALSE(outOfBounds);
                    TEST_ASSERT_GREATER_THAN(0, litPixels());
                }
}

// Both eyes draw the same shape (ANGRY aside), so the right eye must be the
// left eye translated by kEyeRX - kEyeLX — up to ±1 px, since float rounding
// differs slightly at the two x magnitudes (invisible under the 3px brush).
static bool litNear(int x, int y) {
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < eyes::kW && ny >= 0 && ny < eyes::kH &&
                fb[ny][nx])
                return true;
        }
    return false;
}

void test_eyes_are_translated_copies(void) {
    const int shift = eyes::kEyeRX - eyes::kEyeLX;
    for (uint8_t i = 0; i < static_cast<uint8_t>(Expression::_COUNT); ++i) {
        const auto e = static_cast<Expression>(i);
        if (e == Expression::ANGRY) continue;
        drawBoth(e, 1.0f, 0, 0);
        // Window centered on the left eye; x+shift covers the right eye only.
        for (int y = 0; y < eyes::kH; ++y)
            for (int x = eyes::kEyeLX - shift / 2; x < eyes::kEyeLX + shift / 2; ++x) {
                if (fb[y][x])
                    TEST_ASSERT_TRUE_MESSAGE(litNear(x + shift, y), exprName(e));
                if (fb[y][x + shift])
                    TEST_ASSERT_TRUE_MESSAGE(litNear(x, y), exprName(e));
            }
    }
}

// ANGRY: right eye mirrors the left around its own center.
void test_angry_eyes_mirror(void) {
    uint8_t left[eyes::kH][eyes::kW];
    clearFb();
    eyes::drawEye(Expression::ANGRY, false, eyes::kEyeLX, 1.0f, 0, 0, sink, nullptr);
    memcpy(left, fb, sizeof(fb));
    clearFb();
    eyes::drawEye(Expression::ANGRY, true, eyes::kEyeLX, 1.0f, 0, 0, sink, nullptr);
    for (int y = 0; y < eyes::kH; ++y)
        for (int x = 0; x < eyes::kW; ++x) {
            const int mx = 2 * eyes::kEyeLX - x;
            const uint8_t m = (mx >= 0 && mx < eyes::kW) ? left[y][mx] : 0;
            TEST_ASSERT_EQUAL(m, fb[y][x]);
        }
}

// NEUTRAL is an r=18 ring; with the 3px brush the bbox is r=18 ±~2 px.
void test_neutral_bbox_matches_ring(void) {
    clearFb();
    eyes::drawEye(Expression::NEUTRAL, false, eyes::kEyeLX, 1.0f, 0, 0, sink, nullptr);
    const BBox b = bbox();
    TEST_ASSERT_INT_WITHIN(2, eyes::kEyeLX - 18, b.minX);
    TEST_ASSERT_INT_WITHIN(2, eyes::kEyeLX + 18, b.maxX);
    TEST_ASSERT_INT_WITHIN(2, eyes::kCY - 18, b.minY);
    TEST_ASSERT_INT_WITHIN(2, eyes::kCY + 18, b.maxY);
}

// A nearly shut lid collapses every expression to a sliver around kCY.
void test_shut_lid_collapses_height(void) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Expression::_COUNT); ++i) {
        drawBoth(static_cast<Expression>(i), 0.08f, 0, 0);
        const BBox b = bbox();
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(
            8, b.maxY - b.minY, exprName(static_cast<Expression>(i)));
        TEST_ASSERT_INT_WITHIN(4, eyes::kCY, (b.minY + b.maxY) / 2);
    }
}

// Gaze offset is a pure translation of the rendered shape.
void test_gaze_translates_pixels(void) {
    uint8_t centered[eyes::kH][eyes::kW];
    drawBoth(Expression::HAPPY, 1.0f, 0, 0);
    memcpy(centered, fb, sizeof(fb));
    const int gx = 10, gy = 6;  // small enough that nothing clips
    drawBoth(Expression::HAPPY, 1.0f, gx, gy);
    for (int y = 0; y < eyes::kH; ++y)
        for (int x = 0; x < eyes::kW; ++x) {
            const int sx = x - gx, sy = y - gy;
            const uint8_t s = (sx >= 0 && sx < eyes::kW && sy >= 0 && sy < eyes::kH)
                                  ? centered[sy][sx] : 0;
            TEST_ASSERT_EQUAL(s, fb[y][x]);
        }
}

int main(int argc, char** argv) {
    // ASCII dump for eyeballing parity with the browser prototype:
    //   ./test_eyes --dump [expr 0..7] [lid] [gx] [gy]
    if (argc >= 2 && strcmp(argv[1], "--dump") == 0) {
        const auto  e   = static_cast<Expression>(argc > 2 ? atoi(argv[2]) : 0);
        const float lid = argc > 3 ? static_cast<float>(atof(argv[3])) : 1.0f;
        const int   gx  = argc > 4 ? atoi(argv[4]) : 0;
        const int   gy  = argc > 5 ? atoi(argv[5]) : 0;
        drawBoth(e, lid, gx, gy);
        printf("%s lid=%.2f gaze=(%d,%d)\n", exprName(e), lid, gx, gy);
        for (int y = 0; y < eyes::kH; ++y) {
            for (int x = 0; x < eyes::kW; ++x) putchar(fb[y][x] ? '#' : '.');
            putchar('\n');
        }
        return 0;
    }

    UNITY_BEGIN();
    RUN_TEST(test_every_expression_draws_pixels);
    RUN_TEST(test_extreme_gaze_and_lid_stay_in_bounds);
    RUN_TEST(test_eyes_are_translated_copies);
    RUN_TEST(test_angry_eyes_mirror);
    RUN_TEST(test_neutral_bbox_matches_ring);
    RUN_TEST(test_shut_lid_collapses_height);
    RUN_TEST(test_gaze_translates_pixels);
    return UNITY_END();
}
