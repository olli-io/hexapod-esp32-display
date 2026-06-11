#include <Arduino.h>

#include <string.h>

#include "Config.h"
#include "Display.h"
#include "Expression.h"
#include "ExpressionController.h"
#include "Frame.h"
#include "ProtocolCodec.h"
#include "StubRenderer.h"
#include "UartTransport.h"

using namespace proto;

static Display              g_display;
static UartTransport        g_uart;
static ProtocolCodec        g_codec;
static StubRenderer         g_renderer;
static ExpressionController g_controller(g_display, g_renderer);

// ---------------------------------------------------------------------------
// TX helpers
// ---------------------------------------------------------------------------
static void sendFrame(Cmd cmd, const uint8_t* payload = nullptr, uint16_t len = 0) {
    uint8_t buf[ProtocolCodec::frameSize(64)];  // small frames only for v0
    if (len > 64) return;
    const size_t n = ProtocolCodec::encode(cmd, payload, len, buf, sizeof(buf));
    if (n > 0) g_uart.send(buf, n);
}

static void sendAck(Cmd echoCmd) {
    const uint8_t p = static_cast<uint8_t>(echoCmd);
    sendFrame(Cmd::ACK, &p, 1);
}

static void sendNack(NackReason r) {
    const uint8_t p = static_cast<uint8_t>(r);
    sendFrame(Cmd::NACK, &p, 1);
}

static void sendStatus() {
    // Layout: ver_major | ver_minor | ver_patch | uptime_ms (4 LE)
    //       | frames_ok (4 LE) | crc_err (4 LE) | len_err (4 LE)
    //       | dropped_bytes (4 LE) | timeouts (4 LE)
    uint8_t p[3 + 4 + 5 * 4];
    size_t i = 0;
    p[i++] = cfg::FW_VER_MAJOR;
    p[i++] = cfg::FW_VER_MINOR;
    p[i++] = cfg::FW_VER_PATCH;

    auto putU32 = [&](uint32_t v) {
        p[i++] = static_cast<uint8_t>(v       & 0xFF);
        p[i++] = static_cast<uint8_t>((v >> 8) & 0xFF);
        p[i++] = static_cast<uint8_t>((v >> 16) & 0xFF);
        p[i++] = static_cast<uint8_t>((v >> 24) & 0xFF);
    };
    putU32(millis());
    const auto& s = g_codec.stats();
    putU32(s.frames_ok);
    putU32(s.crc_err);
    putU32(s.len_err);
    putU32(s.dropped_bytes);
    putU32(s.timeouts);

    sendFrame(Cmd::STATUS, p, static_cast<uint16_t>(i));
}

// ---------------------------------------------------------------------------
// RX dispatch
// ---------------------------------------------------------------------------
static void handleFrame(Cmd cmd, const uint8_t* payload, uint16_t len) {
    switch (cmd) {
        case Cmd::PING:
            sendFrame(Cmd::PONG, payload, len);  // echo payload
            return;

        case Cmd::SET_EXPRESSION: {
            if (len != 1) { sendNack(NackReason::BAD_PAYLOAD); return; }
            const auto e = static_cast<Expression>(payload[0]);
            if (!g_controller.setExpression(e)) {
                sendNack(NackReason::BAD_PAYLOAD);
                return;
            }
            sendAck(cmd);
            return;
        }

        case Cmd::SET_GAZE: {
            if (len != 1) { sendNack(NackReason::BAD_PAYLOAD); return; }
            const auto g = static_cast<GazeDirection>(payload[0]);
            if (!g_controller.setGaze(g)) {
                sendNack(NackReason::BAD_PAYLOAD);
                return;
            }
            sendAck(cmd);
            return;
        }

        case Cmd::QUERY_STATUS:
            sendStatus();
            return;

        default:
            sendNack(NackReason::UNKNOWN_CMD);
            return;
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);   // USB-CDC debug log
    delay(50);
    log_i("boot: hexapod-display v%u.%u.%u",
          cfg::FW_VER_MAJOR, cfg::FW_VER_MINOR, cfg::FW_VER_PATCH);

    if (!g_display.begin()) log_e("display init failed");
    if (!g_uart.begin(cfg::UART_BAUD, cfg::PIN_UART_RX, cfg::PIN_UART_TX)) {
        log_e("uart init failed");
    }

    g_codec.onFrame(handleFrame);
    g_codec.onError([](NackReason r) { sendNack(r); });
    g_uart.startRxTask([](uint8_t b) { g_codec.feed(b, millis()); });

    g_controller.setExpression(Expression::NEUTRAL);
    g_controller.setGaze(GazeDirection::CENTER);
}

void loop() {
    const uint32_t now = millis();
    g_codec.tick(now);
    g_controller.tick(now);
    delay(1);  // yield to RX task and keep the task WDT happy
}
