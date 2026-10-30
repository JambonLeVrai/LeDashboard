#!/usr/bin/env python3
"""Mock ESP32 endpoint for testing the LeDashboard server without hardware.

Implements the frame protocol from docs/context/COMMUNICATION_PROTOCOL.md:
  SOF(0xAA) | Type(1) | Length(2, big-endian) | Payload(N) | CRC16(2, big-endian) | EOF(0x55)
  CRC-16/CCITT (poly 0x1021, init 0x0000) over Type + Length + Payload.
  Byte stuffing: ESC(0x5A) followed by byte XOR 0xA5 for SOF/EOF/ESC bytes.
  The whole frame body (Type..CRC) is stuffed; the receiver unstuffs every
  byte between SOF and EOF, matching the decoder in the protocol doc.

Behavior: ACKs every valid frame (ACK payload = received type byte),
answers HEARTBEAT with HEARTBEAT, sends NACK(0x02 = CRC_ERROR) on bad CRC.

Run with the project conda environment ONLY:
  C:\\ProgramData\\anaconda3\\envs\\le_dashboard_env\\python.exe \\
      server\\tools\\mock_esp32.py --port COM21

Typical setup: com0com virtual pair COM20 <-> COM21 (see setup_com0com.ps1).
Point the server at COM20, run this mock on COM21.
"""

import argparse
import sys
import time

import serial

SOF = 0xAA
EOF = 0x55
ESC = 0x5A

HEARTBEAT = 0x00
ACK = 0x08
NACK = 0x09
ERROR_CRC = 0x02

TYPE_NAMES = {
    0x00: "HEARTBEAT", 0x01: "TIME", 0x02: "WEATHER", 0x03: "MEDIA",
    0x04: "NOTIFICATION", 0x05: "PORTFOLIO", 0x06: "SYSTEM", 0x07: "CALENDAR",
    0x08: "ACK", 0x09: "NACK", 0x0A: "ERROR", 0x0B: "CONFIG", 0x0C: "REQUEST",
    0x0D: "TOUCH", 0x0E: "COMMAND", 0x0F: "DEBUG", 0x10: "FIRMWARE", 0x11: "OTA",
}


def crc16_ccitt(data: bytes, crc: int = 0x0000) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def stuff(body: bytes) -> bytes:
    out = bytearray()
    for b in body:
        if b in (SOF, EOF, ESC):
            out.append(ESC)
            out.append(b ^ 0xA5)
        else:
            out.append(b)
    return bytes(out)


def encode(msg_type: int, payload: bytes = b"") -> bytes:
    body = bytes([msg_type]) + len(payload).to_bytes(2, "big") + payload
    crc = crc16_ccitt(body)
    body += crc.to_bytes(2, "big")
    return bytes([SOF]) + stuff(body) + bytes([EOF])


class FrameDecoder:
    """Incremental frame decoder; feed() returns (type, payload) tuples for
    valid frames and ("crc_error", raw_type) tuples for corrupt ones."""

    def __init__(self):
        self.reset()

    def reset(self):
        self.state = "WAIT_SOF"
        self.buf = bytearray()
        self.length = 0
        self.esc = False

    def feed(self, data: bytes):
        for raw in data:
            if self.state == "WAIT_SOF":
                if raw == SOF:
                    self.reset()
                    self.state = "READ_BODY"
                continue
            # unstuff every byte between SOF and EOF; the EOF marker itself
            # is only recognized once the full body has been read (the doc's
            # state machine checks EOF only in the READ_EOF state)
            if self.esc:
                b = raw ^ 0xA5
                self.esc = False
            elif raw == ESC:
                self.esc = True
                continue
            elif raw == EOF and self.state == "EXPECT_EOF":
                yield from self._finish()
                continue
            else:
                b = raw
            if self.state == "READ_BODY":
                self.buf.append(b)
                if len(self.buf) == 3:
                    self.length = int.from_bytes(self.buf[1:3], "big")
                    if self.length > 4095:
                        self.reset()
                        continue
                if len(self.buf) == 3 + self.length + 2:
                    self.state = "EXPECT_EOF"
            else:  # stray byte between body and EOF: resync
                self.reset()
                if b == SOF:
                    self.state = "READ_BODY"

    def _finish(self):
        body = bytes(self.buf)
        if len(body) < 5:
            self.reset()
            return
        msg_type = body[0]
        length = int.from_bytes(body[1:3], "big")
        payload = body[3:3 + length]
        received_crc = int.from_bytes(body[3 + length:5 + length], "big")
        calc_crc = crc16_ccitt(body[:3 + length])
        self.reset()
        if calc_crc != received_crc:
            yield ("crc_error", msg_type)
        else:
            yield (msg_type, payload)


def main() -> int:
    ap = argparse.ArgumentParser(description="Mock ESP32 for LeDashboard server testing")
    ap.add_argument("--port", required=True, help="COM port of the emulated ESP32 side (e.g. COM21)")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    print(f"mock_esp32 listening on {args.port} @ {args.baud} baud - Ctrl+C to stop")
    decoder = FrameDecoder()
    try:
        while True:
            data = ser.read(ser.in_waiting or 1)
            if not data:
                continue
            for frame in decoder.feed(data):
                if frame[0] == "crc_error":
                    bad_type = frame[1]
                    print(f"<- CRC ERROR (type=0x{bad_type:02X}) -> NACK")
                    ser.write(encode(NACK, bytes([bad_type, ERROR_CRC])))
                    continue
                msg_type, payload = frame
                name = TYPE_NAMES.get(msg_type, f"0x{msg_type:02X}")
                print(f"<- {name} len={len(payload)} payload={payload[:64].hex()}")
                if msg_type == HEARTBEAT:
                    ser.write(encode(HEARTBEAT))
                    print("-> HEARTBEAT")
                else:
                    ser.write(encode(ACK, bytes([msg_type])))
                    print(f"-> ACK(0x{msg_type:02X})")
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
