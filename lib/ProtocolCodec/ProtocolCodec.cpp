#include "ProtocolCodec.h"

namespace proto {

ProtocolCodec::ProtocolCodec() {
    resetParser();
}

void ProtocolCodec::resetParser() {
    _state = State::WAIT_SOF;
    _len = 0;
    _payloadIdx = 0;
    _rxCrc = 0;
    _crc.reset();
}

void ProtocolCodec::emitError(NackReason r) {
    if (_onError) _onError(r);
}

void ProtocolCodec::feed(uint8_t b, uint32_t nowMs) {
    _lastByteMs = nowMs;

    switch (_state) {
        case State::WAIT_SOF:
            if (b == SOF) {
                _crc.reset();
                _payloadIdx = 0;
                _len = 0;
                _state = State::LEN_LO;
            } else {
                ++_stats.dropped_bytes;
            }
            break;

        case State::LEN_LO:
            _len = b;
            _crc.update(b);
            _state = State::LEN_HI;
            break;

        case State::LEN_HI:
            _len |= static_cast<uint16_t>(b) << 8;
            _crc.update(b);
            if (_len > MAX_PAYLOAD) {
                ++_stats.len_err;
                emitError(NackReason::BAD_LEN);
                resetParser();
            } else {
                _state = State::CMD;
            }
            break;

        case State::CMD:
            _cmd = static_cast<Cmd>(b);
            _crc.update(b);
            _state = (_len == 0) ? State::CRC_HI : State::PAYLOAD;
            break;

        case State::PAYLOAD:
            _payload[_payloadIdx++] = b;
            _crc.update(b);
            if (_payloadIdx >= _len) {
                _state = State::CRC_HI;
            }
            break;

        case State::CRC_HI:
            _rxCrc = static_cast<uint16_t>(b) << 8;
            _state = State::CRC_LO;
            break;

        case State::CRC_LO:
            _rxCrc |= b;
            if (_rxCrc == _crc.value()) {
                ++_stats.frames_ok;
                if (_onFrame) _onFrame(_cmd, _payload, _len);
            } else {
                ++_stats.crc_err;
                emitError(NackReason::BAD_CRC);
            }
            resetParser();
            break;
    }
}

void ProtocolCodec::tick(uint32_t nowMs) {
    if (_state == State::WAIT_SOF) return;
    if (_lastByteMs == 0) return;  // feed() never called with a real timestamp
    if (nowMs - _lastByteMs >= INTER_BYTE_TIMEOUT_MS) {
        ++_stats.timeouts;
        resetParser();
        _lastByteMs = 0;
    }
}

size_t ProtocolCodec::encode(Cmd cmd,
                             const uint8_t* payload, uint16_t len,
                             uint8_t* out, size_t out_cap) {
    if (len > MAX_PAYLOAD) return 0;
    const size_t need = frameSize(len);
    if (out_cap < need) return 0;

    size_t i = 0;
    out[i++] = SOF;
    const uint8_t lenLo = static_cast<uint8_t>(len & 0xFF);
    const uint8_t lenHi = static_cast<uint8_t>((len >> 8) & 0xFF);
    out[i++] = lenLo;
    out[i++] = lenHi;
    out[i++] = static_cast<uint8_t>(cmd);

    Crc16 c;
    c.update(lenLo);
    c.update(lenHi);
    c.update(static_cast<uint8_t>(cmd));
    for (uint16_t k = 0; k < len; ++k) {
        out[i++] = payload[k];
        c.update(payload[k]);
    }
    const uint16_t crc = c.value();
    out[i++] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    out[i++] = static_cast<uint8_t>(crc & 0xFF);
    return i;
}

}  // namespace proto
