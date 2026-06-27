# Commands

Command codes and payloads for the framed binary [protocol](protocol.md).

Pi → ESP32:

- `0x01 PING` — payload: any (echoed back in PONG)
- `0x10 SET_EXPRESSION` — payload: `u8` expression enum
- `0x11 SET_GAZE` — payload: `u8` gaze enum
- `0x12 TRIGGER_BLINK` — payload: empty (one-shot blink)
- `0x20 QUERY_STATUS` — payload: empty

ESP32 → Pi:

- `0x80 ACK` — payload: `u8` echoed command code
- `0x81 NACK` — payload: `u8` NackReason
- `0x82 STATUS` — payload: ver(3) + uptime_ms(4) + 5×u32 stats
- `0x83 PONG` — payload: echo of PING payload
- `0x8F LOG` — payload: UTF-8 string

Enums (name = value):

- `Expression`: `NEUTRAL=0 (0 0)`, `HAPPY=1 (^ ^)`, `SLEEPY=2 (- -)`, `DEAD=3 (x x)`, `GREEDY=4 ($ $)`, `WOOZY=5 (~ ~)`, `ANGRY=6 (> <)`, `LOVE=7 (♥ ♥)`
- `GazeDirection`: `CENTER=0`, `UP=1`, `DOWN=2`, `LEFT=3`, `RIGHT=4`, `UP_LEFT=5`, `UP_RIGHT=6`, `DOWN_LEFT=7`, `DOWN_RIGHT=8`
- `NackReason`: `BAD_CRC=0`, `BAD_LEN=1`, `UNKNOWN_CMD=2`, `BAD_PAYLOAD=3`, `BUSY=4`

## Pi-side test snippet

See [`pi-test-snippet.py`](../pi-test-snippet.py) for a runnable example that
frames a `SET_EXPRESSION` command (with CRC) and reads back the ACK.
