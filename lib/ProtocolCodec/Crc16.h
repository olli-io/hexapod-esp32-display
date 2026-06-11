#pragma once

#include <stdint.h>
#include <stddef.h>

namespace proto {

// CRC-16/CCITT-FALSE
//   poly       = 0x1021
//   init       = 0xFFFF
//   refIn      = false
//   refOut     = false
//   xorOut     = 0x0000
//   check("123456789") == 0x29B1
class Crc16 {
public:
    static constexpr uint16_t POLY = 0x1021;
    static constexpr uint16_t INIT = 0xFFFF;

    constexpr Crc16() = default;

    void reset() { _crc = INIT; }

    void update(uint8_t b) {
        _crc ^= static_cast<uint16_t>(b) << 8;
        for (int i = 0; i < 8; ++i) {
            _crc = (_crc & 0x8000) ? static_cast<uint16_t>((_crc << 1) ^ POLY)
                                   : static_cast<uint16_t>(_crc << 1);
        }
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) update(data[i]);
    }

    uint16_t value() const { return _crc; }

    static uint16_t compute(const uint8_t* data, size_t len) {
        Crc16 c;
        c.update(data, len);
        return c.value();
    }

private:
    uint16_t _crc = INIT;
};

}  // namespace proto
