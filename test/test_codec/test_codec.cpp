#include <unity.h>

#include <string.h>
#include <vector>

#include "ProtocolCodec.h"

using namespace proto;

namespace {

struct Capture {
    Cmd                  cmd = Cmd::PING;
    std::vector<uint8_t> payload;
    int                  frames = 0;

    int                       errors = 0;
    std::vector<NackReason>   errorReasons;
};

Capture g_cap;

void wire(ProtocolCodec& codec) {
    g_cap = {};
    codec.onFrame([&](Cmd c, const uint8_t* p, uint16_t n) {
        g_cap.cmd = c;
        g_cap.payload.assign(p, p + n);
        ++g_cap.frames;
    });
    codec.onError([&](NackReason r) {
        ++g_cap.errors;
        g_cap.errorReasons.push_back(r);
    });
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_encode_decode_roundtrip(void) {
    ProtocolCodec codec;
    wire(codec);

    const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t buf[ProtocolCodec::frameSize(sizeof(payload))];
    const size_t n = ProtocolCodec::encode(Cmd::SET_EXPRESSION,
                                           payload, sizeof(payload),
                                           buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0u, n);
    TEST_ASSERT_EQUAL_UINT(ProtocolCodec::frameSize(sizeof(payload)), n);

    for (size_t i = 0; i < n; ++i) codec.feed(buf[i]);

    TEST_ASSERT_EQUAL_INT(1, g_cap.frames);
    TEST_ASSERT_EQUAL_INT(0, g_cap.errors);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Cmd::SET_EXPRESSION),
                            static_cast<uint8_t>(g_cap.cmd));
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), g_cap.payload.size());
    TEST_ASSERT_EQUAL_MEMORY(payload, g_cap.payload.data(), sizeof(payload));
    TEST_ASSERT_EQUAL_UINT32(1, codec.stats().frames_ok);
}

void test_zero_payload_frame(void) {
    ProtocolCodec codec;
    wire(codec);

    uint8_t buf[ProtocolCodec::frameSize(0)];
    const size_t n = ProtocolCodec::encode(Cmd::PING, nullptr, 0,
                                           buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT(6u, n);  // SOF + LEN(2) + CMD + CRC(2)

    for (size_t i = 0; i < n; ++i) codec.feed(buf[i]);

    TEST_ASSERT_EQUAL_INT(1, g_cap.frames);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Cmd::PING),
                            static_cast<uint8_t>(g_cap.cmd));
    TEST_ASSERT_EQUAL_UINT(0u, g_cap.payload.size());
}

void test_bad_crc_emits_nack(void) {
    ProtocolCodec codec;
    wire(codec);

    const uint8_t payload[] = {0xAA};
    uint8_t buf[ProtocolCodec::frameSize(sizeof(payload))];
    const size_t n = ProtocolCodec::encode(Cmd::SET_GAZE,
                                           payload, sizeof(payload),
                                           buf, sizeof(buf));
    // Corrupt the CRC low byte.
    buf[n - 1] ^= 0xFF;

    for (size_t i = 0; i < n; ++i) codec.feed(buf[i]);

    TEST_ASSERT_EQUAL_INT(0, g_cap.frames);
    TEST_ASSERT_EQUAL_INT(1, g_cap.errors);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(NackReason::BAD_CRC),
                            static_cast<uint8_t>(g_cap.errorReasons[0]));
    TEST_ASSERT_EQUAL_UINT32(1, codec.stats().crc_err);
}

void test_bad_len_emits_nack(void) {
    ProtocolCodec codec;
    wire(codec);

    // Hand-craft: SOF, LEN = MAX_PAYLOAD + 1 (LE), then garbage.
    const uint16_t bigLen = MAX_PAYLOAD + 1;
    codec.feed(SOF);
    codec.feed(static_cast<uint8_t>(bigLen & 0xFF));
    codec.feed(static_cast<uint8_t>((bigLen >> 8) & 0xFF));

    TEST_ASSERT_EQUAL_INT(0, g_cap.frames);
    TEST_ASSERT_EQUAL_INT(1, g_cap.errors);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(NackReason::BAD_LEN),
                            static_cast<uint8_t>(g_cap.errorReasons[0]));
    TEST_ASSERT_EQUAL_UINT32(1, codec.stats().len_err);
}

void test_garbage_does_not_produce_frame(void) {
    ProtocolCodec codec;
    wire(codec);

    // Pseudo-random garbage, no SOF byte in it.
    for (int i = 0; i < 1000; ++i) {
        uint8_t b = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
        if (b == SOF) b ^= 1;
        codec.feed(b);
    }

    TEST_ASSERT_EQUAL_INT(0, g_cap.frames);
    TEST_ASSERT_EQUAL_INT(0, g_cap.errors);
    TEST_ASSERT_EQUAL_UINT32(1000, codec.stats().dropped_bytes);
}

void test_resync_after_garbage(void) {
    ProtocolCodec codec;
    wire(codec);

    // Inject garbage, then a clean frame. Parser should resync on SOF.
    for (int i = 0; i < 32; ++i) codec.feed(0x00);

    const uint8_t payload[] = {0x05};
    uint8_t buf[ProtocolCodec::frameSize(1)];
    const size_t n = ProtocolCodec::encode(Cmd::SET_GAZE,
                                           payload, 1, buf, sizeof(buf));
    for (size_t i = 0; i < n; ++i) codec.feed(buf[i]);

    TEST_ASSERT_EQUAL_INT(1, g_cap.frames);
    TEST_ASSERT_EQUAL_UINT8(0x05, g_cap.payload[0]);
}

void test_inter_byte_timeout(void) {
    ProtocolCodec codec;
    wire(codec);

    // Start a frame but stall mid-payload.
    codec.feed(SOF,          100);
    codec.feed(0x04,         101);  // len lo = 4
    codec.feed(0x00,         102);  // len hi
    codec.feed(static_cast<uint8_t>(Cmd::SET_EXPRESSION), 103);
    codec.feed(0xDE,         104);  // 1/4 payload bytes
    // Then go silent — advance clock past the timeout.
    codec.tick(104 + ProtocolCodec::INTER_BYTE_TIMEOUT_MS + 5);

    TEST_ASSERT_EQUAL_UINT32(1, codec.stats().timeouts);

    // After timeout we should be able to start a fresh frame cleanly.
    const uint8_t payload[] = {0xAB};
    uint8_t buf[ProtocolCodec::frameSize(1)];
    const size_t n = ProtocolCodec::encode(Cmd::PING, payload, 1, buf, sizeof(buf));
    for (size_t i = 0; i < n; ++i) codec.feed(buf[i], 200 + static_cast<uint32_t>(i));

    TEST_ASSERT_EQUAL_INT(1, g_cap.frames);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Cmd::PING),
                            static_cast<uint8_t>(g_cap.cmd));
}

void test_encode_rejects_oversize(void) {
    uint8_t out[8];
    const size_t n = ProtocolCodec::encode(Cmd::PING, nullptr, MAX_PAYLOAD + 1,
                                           out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(0u, n);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_decode_roundtrip);
    RUN_TEST(test_zero_payload_frame);
    RUN_TEST(test_bad_crc_emits_nack);
    RUN_TEST(test_bad_len_emits_nack);
    RUN_TEST(test_garbage_does_not_produce_frame);
    RUN_TEST(test_resync_after_garbage);
    RUN_TEST(test_inter_byte_timeout);
    RUN_TEST(test_encode_rejects_oversize);
    return UNITY_END();
}
