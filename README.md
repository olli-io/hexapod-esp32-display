# hexapod-esp32-display

> [!WARNING]
> Work in progress — This project is under active development. APIs, configuration, and behavior may change without notice, and some features are incomplete or untested. Use at your own risk.

**This repository is part of a multi-repo hexapod stack:**
- ROS2 hexapod controller - ['olli-io/hexapod-ros2-control'](https://github.com/olli-io/hexapod-ros2-control)
- Driver firmware for Pimoroni hexapod-servo2040-driver ['olli-io/hexapod-servo2040-driver'](https://github.com/olli-io/hexapod-servo2040-driver)

Firmware for the "face" of a hexapod robot. A Seeed XIAO ESP32-C3 drives a
256×64 SH1122 OLED to display emoji-like expressions (`^ ^`, `O O`, `- -`, …)
and gaze directions (up/down/left/right/…). Commands arrive from a Raspberry
Pi over UART using a small framed binary protocol.

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
make -C test sim               # interactive terminal eye simulator
```

## Hardware

- **MCU**: Seeed XIAO ESP32-C3
- **Display**: 256×64 OLED, SH1122 controller, 4-wire SPI
- **Host**: Raspberry Pi over UART, common ground

### Wiring — XIAO ESP32-C3 → SH1122 OLED

Format: `XIAO pad (GPIO) -> SH1122 pin`

- `3V3 -> VCC`
- `GND -> GND`
- `D8 (GPIO8) -> SCK / D0`
- `D10 (GPIO10) -> MOSI / D1`
- `D3 (GPIO5) -> CS`
- `D2 (GPIO4) -> DC`
- `D1 (GPIO3) -> RST`

### Wiring — XIAO ESP32-C3 → Raspberry Pi (40-pin header)

Format: `XIAO pad (GPIO) -> Pi pin`

- `GND -> GND (pin 6)`
- `D6 (GPIO21) -> RX (pin 10, GPIO15)`
- `D7 (GPIO20) -> TX (pin 8, GPIO14)`

On Pi 3/4, enable the PL011 UART and free it from Bluetooth via
`/boot/config.txt` (or `/boot/firmware/config.txt`): `enable_uart=1` and

### Pin notes

- `D6/D7` are silkscreen "UART0" pins, but debug logs use the C3's built-in
  USB Serial/JTAG (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`), leaving UART1 on
  these pins free for the Pi link.
- `D8` (GPIO8) is a strapping pin that must be HIGH at boot; SPI mode-0 SCK
  idles low after bus init, so boot-time state is fine.
- `D9` (GPIO9, BOOT button) is left free.

## Protocol

- [docs/protocol.md](docs/protocol.md) — frame format, CRC, link supervision
- [docs/commands.md](docs/commands.md) — command codes, payloads, enums

## Project layout

ESP-IDF component layout; each module is an IDF component.

```
main/                       main.cpp (wiring + dispatch, app_main + tick loop)
components/config/          Config.h (pin map, baud, version)
components/display/         u8g2 wrapper, SPI init, RST pulse
components/uart_transport/  UART1 driver owner (main-loop pump, no RX task)
components/protocol/        Frame format, CRC16, parser SM (platform-free)
components/expression/      target state + ~60 Hz render tick + IRenderer
components/renderer/        EyeRenderer + EyeRaster (host-testable) + StubRenderer
components/u8g2/            u8g2 C core (git submodule) + ESP-IDF SPI/GPIO HAL
test/                       host-side Unity tests (make -C test)
```

`protocol` has no ESP-IDF dependency and compiles natively (vendored Unity,
plain Makefile) for millisecond host-side tests.

## Raspberry Pi direct face (ROS2)

`ros2/face/` is an `ament_cmake` package (`hexapod_face`) that drives the SH1122
**directly from the Pi** — no ESP32 in the loop. It reuses the platform-free
animation core (`EyeAnim` + `EyeRaster`) verbatim from this repo and builds a
single self-contained panel driver around `u8g2`'s built-in `sh1122_256x64`:
pixel bytes over `/dev/spidev`, and DC/RST/CS over the Linux GPIO character
device (kernel uABI ioctls — **no libgpiod or other GPIO library**). This is the
path used on the robot; the ESP-IDF firmware above stays buildable but is no
longer the face.

```
ros2/face/src/Sh1122Panel.{h,cpp}  panel driver: u8g2 + spidev + kernel GPIO uABI
ros2/face/src/FaceNames.h          case-insensitive name<->enum (reuses firmware names)
ros2/face/src/face_node.cpp        rclcpp node: render timer + expression/gaze/blink topics
ros2/face/config/face.yaml         spi_device, gpio_chip, dc/rst/cs lines, render_hz, headless
```

Design notes:
- Built on `EyeAnim`/`EyeRaster` directly (no `Display`/`IRenderer`/`Expression`
  `Controller`), so there is no ESP-only link-supervision baggage on the Pi.
- Per-instance transport via u8g2's `user_ptr` — no file-scope globals.
- **Dirty-flush**: a frame is pushed over SPI only when it actually changed, so a
  static face costs no bus traffic between blinks/gaze moves.
- `headless:=true` runs the full pipeline without touching SPI/GPIO (CI / dev box);
  the same path is covered by `test/test_face` in `make -C test`.

### Wiring — Raspberry Pi (SPI0) → SH1122 OLED

Format: `Pi pin (BCM GPIO) -> SH1122 pin`. DC/RST/CS GPIOs are configurable in
`config/face.yaml` (defaults shown).

- `3V3 -> VCC`
- `GND -> GND`
- `SCLK (GPIO11, pin 23) -> SCK / D0`
- `MOSI (GPIO10, pin 19) -> MOSI / D1`
- `GPIO8 (CE0, pin 24) -> CS`     (`cs_line: 8`; driven manually, SPI_NO_CS)
- `GPIO24 (pin 18) -> DC`         (`dc_line: 24`)
- `GPIO25 (pin 22) -> RST`        (`rst_line: 25`)

Enable SPI in `/boot/firmware/config.txt`: `dtparam=spi=on`, then reboot.

### Build & run (on the Pi)

```sh
# needs a ROS2 distro (rclcpp, std_msgs); no extra system libraries
ln -s "$PWD/ros2/face" ~/ros2_ws/src/         # bring the package into a colcon ws
cd ~/ros2_ws && colcon build --packages-select hexapod_face
source install/setup.bash
ros2 launch hexapod_face face.launch.py       # NEUTRAL eyes + idle blinks
```

The control node sets the target expression/gaze in-process; for manual /
bring-up control the same states are string topics:

```sh
ros2 topic pub --once /face_node/expression std_msgs/String "{data: HAPPY}"
ros2 topic pub --once /face_node/gaze       std_msgs/String "{data: UP_LEFT}"
ros2 topic pub --once /face_node/blink      std_msgs/Empty  "{}"
```

Names match the [`commands.md`](docs/commands.md) enums (case-insensitive). The
user running the node needs access to `/dev/spidev*` and `/dev/gpiochip*` (groups
`spi`/`gpio`, or a udev rule).

## Eye animation

`EyeRenderer` runs autonomously on top of the target state set over UART:

- Expression changes transition through a 260 ms blink, swapping artwork at
  the midpoint (eyes shut) so switches never pop.
- Gaze eases to its target over 220 ms (easeOutCubic), up to ±20/±12 px,
  applied at the pixel level so it composes with blinks.
- Idle blinks fire every 2.2–5.2 s (hardware RNG); `TRIGGER_BLINK` (0x12)
  forces a one-shot blink.

`EyeRaster` is platform-free — shapes rasterized with line/bezier/arc/fill
primitives and a 3 px brush into the 256×64 1-bit buffer, covered by host
tests (`test/test_eyes`, with an ASCII dump mode: `./build/test_eyes --dump
<expr 0-7> [lid] [gx] [gy]`).

`make -C test sim` runs an interactive terminal simulator (≥96 columns;
Braille cells on short terminals, quadrant blocks otherwise) driving the same
`EyeAnim` + `EyeRaster` code as the firmware — keys 0-7 switch expressions,
arrows/wasd move gaze, space blinks, q quits.
