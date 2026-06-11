#pragma once

#include <stddef.h>
#include <stdint.h>
#include <functional>

#include "Frame.h"
#include "Crc16.h"

namespace proto {

class ProtocolCodec {
public:
    using FrameHandler =
        std::function<void(Cmd cmd, const uint8_t* payload, uint16_t len)>;
    using ErrorHandler = std::function<void(NackReason)>;

    struct Stats {
        uint32_t frames_ok      = 0;
        uint32_t crc_err        = 0;
        uint32_t len_err        = 0;
        uint32_t dropped_bytes  = 0;
        uint32_t timeouts       = 0;
    };

    ProtocolCodec();

    void onFrame(FrameHandler h) { _onFrame = std::move(h); }
    void onError(ErrorHandler h) { _onError = std::move(h); }

    // Feed one received byte. Drives the parser state machine.
    // nowMs is current monotonic ms (or 0 in host tests that don't care).
    void feed(uint8_t b, uint32_t nowMs = 0);

    // Inter-byte watchdog. Pass current monotonic ms. If we have been
    // mid-frame for longer than INTER_BYTE_TIMEOUT_MS, reset to WAIT_SOF.
    // Safe to call from a 1 ms loop tick.
    void tick(uint32_t nowMs);

    // Encode a frame into `out`. Returns bytes written, or 0 on failure
    // (out_cap too small or len > MAX_PAYLOAD).
    static size_t encode(Cmd cmd,
                         const uint8_t* payload, uint16_t len,
                         uint8_t* out, size_t out_cap);

    // Minimum buffer size required to encode a frame with `payloadLen` bytes.
    static constexpr size_t frameSize(uint16_t payloadLen) {
        // SOF(1) + LEN(2) + CMD(1) + PAYLOAD(N) + CRC(2)
        return 1u + 2u + 1u + payloadLen + 2u;
    }

    const Stats& stats() const { return _stats; }
    void resetStats() { _stats = {}; }

    static constexpr uint32_t INTER_BYTE_TIMEOUT_MS = 50;

private:
    enum class State : uint8_t {
        WAIT_SOF,
        LEN_LO,
        LEN_HI,
        CMD,
        PAYLOAD,
        CRC_HI,
        CRC_LO,
    };

    void resetParser();
    void emitError(NackReason r);

    State        _state = State::WAIT_SOF;
    uint16_t     _len   = 0;
    uint16_t     _payloadIdx = 0;
    Cmd          _cmd   = Cmd::PING;
    Crc16        _crc;
    uint16_t     _rxCrc = 0;
    uint32_t     _lastByteMs = 0;
    uint8_t      _payload[MAX_PAYLOAD];

    FrameHandler _onFrame;
    ErrorHandler _onError;
    Stats        _stats;
};

}  // namespace proto
