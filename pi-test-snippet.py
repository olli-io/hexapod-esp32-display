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
