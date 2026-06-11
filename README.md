# hexapod-esp32-display

Firmware for the "face" of a hexapod robot. A Seeed XIAO ESP32-C3 drives a
256×64 SH1122 OLED to display emoji-like expressions (`^ ^`, `O O`, `- -`, …)
and gaze directions (up/down/left/right/…). Commands arrive from a Raspberry
Pi over UART using a small framed binary protocol.

This is the v0 boilerplate: full transport, codec, and dispatch pipeline are
wired up; eye graphics are stubbed out with a text label so the whole loop
can be smoke-tested before any art is added.

## Hardware

- **MCU**: Seeed XIAO ESP32-C3
- **Display**: 256×64 OLED, SH1122 controller, 4-wire SPI
- **Host**: Raspberry Pi over UART, common ground

### Wiring — XIAO ESP32-C3 → SH1122 OLED

```
XIAO pad      GPIO     SH1122
─────────────────────────────
3V3                ─── VCC
GND                ─── GND
D8         GPIO8   ─── SCK / D0
D10        GPIO10  ─── MOSI / D1
D3         GPIO5   ─── CS
D2         GPIO4   ─── DC
D1         GPIO3   ─── RST
```

### Wiring — XIAO ESP32-C3 → Raspberry Pi (40-pin header)

```
XIAO pad      GPIO     Pi
─────────────────────────────
GND                ─── GND  (pin 6)
D6         GPIO21  ─── RX   (pin 10, GPIO15)
D7         GPIO20  ─── TX   (pin 8,  GPIO14)
```

On Pi 3/4, enable the PL011 UART and free it from Bluetooth:

```
# /boot/config.txt (or /boot/firmware/config.txt on newer Pi OS)
enable_uart=1
dtoverlay=disable-bt
```

### Pin notes

- `D6/D7` are silkscreen "UART0" pins on the XIAO. Debug logs go out over
  the C3's built-in USB Serial/JTAG port (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`),
  so UART1 on these pins is free for the Pi link.
- `D8` (GPIO8) is a strapping pin and must be HIGH at boot. SPI mode-0 SCK
  idles low after bus init — boot-time state is fine.
- `D9` (GPIO9, BOOT button) is left free.

## Build, flash, test

Built with plain ESP-IDF (v5.5.x). One-time setup:

```sh
git clone --branch v5.5.4 --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32c3
git submodule update --init      # vendored u8g2 in components/u8g2/u8g2
```

Then, in each shell:

```sh
. ~/esp/esp-idf/export.sh
idf.py build                   # build firmware
idf.py flash                   # flash via USB-C
idf.py monitor                 # USB Serial/JTAG log (Ctrl-] to quit)
make -C test                   # host-side codec + CRC tests
```

## Protocol

Framed binary, little-endian length, big-endian CRC. CRC-16/CCITT-FALSE
(poly `0x1021`, init `0xFFFF`, no reflection, no xor-out;
`CRC("123456789") == 0x29B1`) computed over `LEN + CMD + PAYLOAD`.

```
+-------+------+------+-----+--------------+-------+-------+
| SOF   | LEN_LO | LEN_HI | CMD | PAYLOAD[LEN] | CRC_H | CRC_L |
| 0xA5  | u8     | u8     | u8  |    bytes     | u8    | u8    |
+-------+------+------+-----+--------------+-------+-------+
```

`MAX_PAYLOAD = 1024`. Inter-byte timeout = 50 ms; a Pi reboot mid-frame
resets the parser cleanly.

### Commands

| Code | Direction     | Name           | Payload                              |
|------|---------------|----------------|--------------------------------------|
| 0x01 | Pi → ESP32    | PING           | any (echoed back in PONG)            |
| 0x10 | Pi → ESP32    | SET_EXPRESSION | `u8` expression enum                 |
| 0x11 | Pi → ESP32    | SET_GAZE       | `u8` gaze enum                       |
| 0x20 | Pi → ESP32    | QUERY_STATUS   | empty                                |
| 0x80 | ESP32 → Pi    | ACK            | `u8` echoed command code             |
| 0x81 | ESP32 → Pi    | NACK           | `u8` NackReason                      |
| 0x82 | ESP32 → Pi    | STATUS         | ver(3) + uptime_ms(4) + 5×u32 stats  |
| 0x83 | ESP32 → Pi    | PONG           | echo of PING payload                 |
| 0x8F | ESP32 → Pi    | LOG            | UTF-8 string                         |

`Expression`: `NEUTRAL=0, HAPPY=1, SLEEPY=2, SURPRISED=3, ANGRY=4, SAD=5, BLINK=6`
`GazeDirection`: `CENTER=0, UP=1, DOWN=2, LEFT=3, RIGHT=4, UP_LEFT=5, UP_RIGHT=6, DOWN_LEFT=7, DOWN_RIGHT=8`
`NackReason`: `BAD_CRC=0, BAD_LEN=1, UNKNOWN_CMD=2, BAD_PAYLOAD=3, BUSY=4`

### Worked example

A `PING` with no payload:

```
A5 00 00 01 ?? ??
```

CRC is over the four bytes `00 00 01` (LEN_LO, LEN_HI, CMD); compute
`Crc16::compute(...)` to fill in the last two bytes (big-endian).

### Pi-side test snippet

```python
# pip install pyserial
import serial, struct

SOF = 0xA5
def crc16(data, init=0xFFFF, poly=0x1021):
    c = init
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c

def frame(cmd, payload=b''):
    body = struct.pack('<H', len(payload)) + bytes([cmd]) + payload
    c = crc16(body)
    return bytes([SOF]) + body + bytes([(c >> 8) & 0xFF, c & 0xFF])

with serial.Serial('/dev/serial0', 921600, timeout=0.2) as s:
    s.write(frame(0x10, bytes([1])))  # SET_EXPRESSION HAPPY
    print(s.read(16).hex())           # expect ACK frame
```

## Project layout

ESP-IDF component layout; each module is an IDF component.

```
main/                   main.cpp (wiring + dispatch, app_main + tick loop)
components/config/      Config.h (pin map, baud, version)
components/display/     u8g2 wrapper, SPI init, RST pulse
components/uart_transport/  UART1 driver owner (main-loop pump, no RX task)
components/protocol/    Frame format, CRC16, parser SM (platform-free)
components/expression/  state + 60 Hz dirty-bit render gate + IRenderer
components/renderer/    v0 StubRenderer
components/u8g2/        u8g2 C core (git submodule) + ESP-IDF SPI/GPIO HAL
test/                   host-side Unity tests (make -C test)
```

`protocol` has no ESP-IDF dependency and compiles natively (vendored Unity,
plain Makefile) for millisecond host-side tests.

## Roadmap

- Real eye renderer (`EyeRenderer : IRenderer`) with parametric pupils.
- Expression-to-expression interpolation animation; saccade-style gaze
  transitions.
- Periodic `STATUS` push so the Pi can monitor uptime / codec health.
- I²C IMU (D4/D5) for head-tilt-driven gaze drift.
- Second display variant (back-of-head LED matrix?) — at that point
  introduce a real `ICanvas` abstraction over U8g2.
