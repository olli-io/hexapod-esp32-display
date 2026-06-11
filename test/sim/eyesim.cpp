// Terminal eye simulator: runs the firmware's EyeAnim + EyeRaster on the
// host. The framebuffer is cropped to the 192 px the eyes can actually
// reach and rendered with Unicode quadrant blocks (2×2 px per character →
// 96×32 cells); on terminals too short for that it falls back to Braille
// cells (2×4 px per character → 96×16).
//
//   make sim        (from test/)
//
// Keys: 0-7 expression · arrows/wasd gaze · c center · space blink · q quit

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "EyeAnim.h"
#include "EyeRaster.h"

static uint8_t fb[eyes::kH][eyes::kW];

static void sink(void*, int x, int y) { fb[y][x] = 1; }

static uint32_t nowMs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static uint32_t hostRand() { return static_cast<uint32_t>(rand()); }

static const char* simExprName(Expression e) {
    static const char* names[] = {"NEUTRAL", "HAPPY",  "SLEEPY", "DEAD",
                                  "GREEDY",  "WOOZY",  "ANGRY",  "LOVE"};
    return names[static_cast<size_t>(e)];
}

static const char* simGazeName(GazeDirection g) {
    static const char* names[] = {"CENTER",  "UP",       "DOWN",      "LEFT",
                                  "RIGHT",   "UP_LEFT",  "UP_RIGHT",
                                  "DOWN_LEFT", "DOWN_RIGHT"};
    return names[static_cast<size_t>(g)];
}

// ---------------------------------------------------------------------------
// Terminal handling
// ---------------------------------------------------------------------------
static termios g_origTermios;

static void restoreTerminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_origTermios);
    fputs("\x1b[?25h\x1b[?7h\x1b[0m\n", stdout);  // cursor, autowrap, color
    fflush(stdout);
}

static void onSignal(int) {
    restoreTerminal();
    _exit(0);
}

static void setupTerminal() {
    tcgetattr(STDIN_FILENO, &g_origTermios);
    termios raw = g_origTermios;
    raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;  // non-blocking reads
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    atexit(restoreTerminal);
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    fputs("\x1b[2J\x1b[?25l\x1b[?7l", stdout);  // clear, hide cursor, no autowrap
}

// ---------------------------------------------------------------------------
// Rendering. Even at max gaze (±20 px) plus the 3 px brush, the eyes stay
// inside x ∈ [34, 222], so only that 192 px window is shown (96 columns).
// ---------------------------------------------------------------------------
constexpr int kCropX = 32;
constexpr int kCropW = 192;

static int px(int x, int y) { return fb[y][x + kCropX]; }

static const char* kQuad[16] = {
    " ", "▘", "▝", "▀", "▖", "▌", "▞", "▛",
    "▗", "▚", "▐", "▜", "▄", "▙", "▟",
    "█"};

static void drawFrame(const RenderState& state) {
    winsize ws{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    // Quadrant blocks need 32 rows + status; fall back to Braille (16 rows).
    const bool braille = ws.ws_row != 0 && ws.ws_row < eyes::kH / 2 + 2;

    static char out[32 * 1024];
    char* p = out;
    p += sprintf(p, "\x1b[H");
    if (braille) {
        for (int cy = 0; cy < eyes::kH / 4; ++cy) {
            for (int cx = 0; cx < kCropW / 2; ++cx) {
                const int x = cx * 2, y = cy * 4;
                const int b = px(x, y) | px(x, y + 1) << 1 | px(x, y + 2) << 2 |
                              px(x + 1, y) << 3 | px(x + 1, y + 1) << 4 |
                              px(x + 1, y + 2) << 5 | px(x, y + 3) << 6 |
                              px(x + 1, y + 3) << 7;
                // UTF-8 for U+2800 + b
                *p++ = '\xE2';
                *p++ = static_cast<char>(0xA0 | (b >> 6));
                *p++ = static_cast<char>(0x80 | (b & 0x3F));
            }
            p += sprintf(p, "\x1b[K\n");
        }
    } else {
        for (int cy = 0; cy < eyes::kH / 2; ++cy) {
            for (int cx = 0; cx < kCropW / 2; ++cx) {
                const int x   = cx * 2, y = cy * 2;
                const int idx = px(x, y) | px(x + 1, y) << 1 |
                                px(x, y + 1) << 2 | px(x + 1, y + 1) << 3;
                p += sprintf(p, "%s", kQuad[idx]);
            }
            p += sprintf(p, "\x1b[K\n");
        }
    }
    p += sprintf(p,
                 "\x1b[K EXPR %-7s GAZE %-10s "
                 "0-7 expr · wasd gaze · c center · space blink · q quit\x1b[J",
                 simExprName(state.expr), simGazeName(state.gaze));
    fwrite(out, 1, static_cast<size_t>(p - out), stdout);
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static bool handleKeys(RenderState& state, eyes::EyeAnim& anim) {
    uint8_t buf[16];
    const ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    for (ssize_t i = 0; i < n; ++i) {
        uint8_t c = buf[i];
        if (c == 0x1b && i + 2 < n && buf[i + 1] == '[') {  // arrow keys
            switch (buf[i + 2]) {
                case 'A': c = 'w'; break;
                case 'B': c = 's'; break;
                case 'C': c = 'd'; break;
                case 'D': c = 'a'; break;
                default:  c = 0;   break;
            }
            i += 2;
        }
        if (c >= '0' && c <= '7') state.expr = static_cast<Expression>(c - '0');
        switch (c) {
            case 'w': state.gaze = GazeDirection::UP; break;
            case 's': state.gaze = GazeDirection::DOWN; break;
            case 'a': state.gaze = GazeDirection::LEFT; break;
            case 'd': state.gaze = GazeDirection::RIGHT; break;
            case 'c': state.gaze = GazeDirection::CENTER; break;
            case ' ': anim.requestBlink(); break;
            case 'q': return false;
        }
    }
    return true;
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    setupTerminal();

    eyes::EyeAnim anim(hostRand);
    RenderState   state{Expression::NEUTRAL, GazeDirection::CENTER};

    while (handleKeys(state, anim)) {
        const eyes::AnimFrame f = anim.update(state, nowMs());
        memset(fb, 0, sizeof(fb));
        eyes::drawEye(f.expr, false, eyes::kEyeLX, f.lid, f.gx, f.gy, sink, nullptr);
        eyes::drawEye(f.expr, true,  eyes::kEyeRX, f.lid, f.gx, f.gy, sink, nullptr);
        drawFrame(state);
        usleep(33000);  // ~30 fps; animation timing is wall-clock anyway
    }
    return 0;
}
