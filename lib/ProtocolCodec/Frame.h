#pragma once

#include <stdint.h>

namespace proto {

// Start-of-frame byte. Chosen for good Hamming distance from 0x00/0xFF and
// because it is uncommon in ASCII traffic.
constexpr uint8_t  SOF         = 0xA5;

// Maximum PAYLOAD length (excludes CMD and CRC). Two-byte LEN on the wire
// allows up to 65535, but we cap firmware-side to keep RAM use bounded.
constexpr uint16_t MAX_PAYLOAD = 1024;

// On-wire frame:
//   SOF(1) | LEN(2 LE, payload-only) | CMD(1) | PAYLOAD(LEN) | CRC16(2 BE)
// CRC-16/CCITT-FALSE over LEN + CMD + PAYLOAD.

enum class Cmd : uint8_t {
    // Pi -> ESP32
    PING            = 0x01,
    SET_EXPRESSION  = 0x10,
    SET_GAZE        = 0x11,
    QUERY_STATUS    = 0x20,

    // ESP32 -> Pi
    ACK             = 0x80,
    NACK            = 0x81,
    STATUS          = 0x82,
    PONG            = 0x83,
    LOG             = 0x8F,
};

enum class NackReason : uint8_t {
    BAD_CRC      = 0,
    BAD_LEN      = 1,
    UNKNOWN_CMD  = 2,
    BAD_PAYLOAD  = 3,
    BUSY         = 4,
};

}  // namespace proto
