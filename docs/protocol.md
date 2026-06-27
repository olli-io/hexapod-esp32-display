# Protocol

Framed binary, little-endian length, big-endian CRC. CRC-16/CCITT-FALSE
(poly `0x1021`, init `0xFFFF`, no reflection, no xor-out;
`CRC("123456789") == 0x29B1`) over `LEN + CMD + PAYLOAD`.

Frame layout, in byte order:

1. `SOF` — `u8`, always `0xA5`
2. `LEN_LO` — `u8`, payload length low byte
3. `LEN_HI` — `u8`, payload length high byte
4. `CMD` — `u8`, command code
5. `PAYLOAD` — `LEN` bytes
6. `CRC_H` — `u8`, CRC high byte
7. `CRC_L` — `u8`, CRC low byte

`MAX_PAYLOAD = 1024`. Inter-byte timeout = 50 ms, so a Pi reboot mid-frame
resets the parser cleanly. Any valid frame counts as link activity; if none
arrives for 3 s (`LINK_TIMEOUT_MS`) the eyes fall back to `DEAD` until the
link returns, then the last expression/gaze resume. Send `PING` (or anything)
every ~1 s to keep the link alive.

A payload-less `PING` is `A5 00 00 01 ?? ??`, where the last two bytes are
`Crc16::compute(...)` over `00 00 01` (big-endian).

See [commands.md](commands.md) for the command codes, payloads, and enums.
