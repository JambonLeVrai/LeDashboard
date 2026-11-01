#!/usr/bin/env python3
"""Full ESP32 emulator for testing the LeDashboard C++ server.

Emulates the ESP32 firmware's serial protocol:
  - Sends periodic HEARTBEAT (every 5s)
  - Responds to HEARTBEAT with HEARTBEAT
  - Receives TIME messages, stores and displays them, sends ACK
  - Sends REQUEST(time_sync) on connect
  - Sends ACK for any valid frame, NACK for CRC errors
  - Optionally sends TOUCH events for testing

Logs every frame (sent and received) with timestamps to both console and a log file.

Protocol (docs/context/COMMUNICATION_PROTOCOL.md):
  SOF(0xAA) | Type(1) | Length(2, BE) | Payload(N) | CRC16-CCITT(2, BE) | EOF(0x55)
  Byte stuffing: ESC(0x5A) + (byte XOR 0xA5) for SOF/EOF/ESC in the frame body.

Usage:
  C:\\ProgramData\\anaconda3\\envs\\le_dashboard_env\\python.exe \\
      server\\tools\\test_esp32_emulator.py --port COM6 [--baud 115200] [--log emulator.log]

Typical setup: com0com virtual pair COM5 <-> COM6.
  Server on COM5, emulator on COM6 (or vice versa).
"""

import argparse
import datetime
import os
import sys
import time

import serial

# =============================================================================
# Protocol constants
# =============================================================================

SOF = 0xAA
EOF = 0x55
ESC = 0x5A

HEARTBEAT    = 0x00
TIME         = 0x01
WEATHER      = 0x02
MEDIA        = 0x03
NOTIFICATION = 0x04
PORTFOLIO    = 0x05
SYSTEM       = 0x06
CALENDAR     = 0x07
ACK          = 0x08
NACK         = 0x09
ERROR        = 0x0A
CONFIG       = 0x0B
REQUEST      = 0x0C
TOUCH        = 0x0D
COMMAND      = 0x0E
DEBUG        = 0x0F

TYPE_NAMES = {
    0x00: "HEARTBEAT", 0x01: "TIME", 0x02: "WEATHER", 0x03: "MEDIA",
    0x04: "NOTIFICATION", 0x05: "PORTFOLIO", 0x06: "SYSTEM", 0x07: "CALENDAR",
    0x08: "ACK", 0x09: "NACK", 0x0A: "ERROR", 0x0B: "CONFIG", 0x0C: "REQUEST",
    0x0D: "TOUCH", 0x0E: "COMMAND", 0x0F: "DEBUG",
    0x80: "DISCOVER", 0x81: "DISCOVER_ACK",
}

DISCOVER = 0x80
DISCOVER_ACK = 0x81
DISCOVER_MAGIC = b"LEDASHBOARD_PROBE"
DISCOVER_ACK_PAYLOAD = b"LEDASHBOARD_ESP32_V1"

ERROR_NAMES = {
    0x00: "SUCCESS", 0x01: "INVALID_FORMAT", 0x02: "CRC_ERROR", 0x03: "TIMEOUT",
    0x04: "BUFFER_OVERFLOW", 0x05: "NOT_IMPLEMENTED", 0x06: "INVALID_STATE",
    0x07: "RESOURCE_ERROR", 0x08: "COMMUNICATION_ERROR", 0x09: "PARSING_ERROR",
}

REQUEST_NAMES = {
    0x00: "FULL_SYNC", 0x01: "TIME_SYNC", 0x02: "WEATHER_UPDATE",
    0x03: "MEDIA_UPDATE", 0x04: "NOTIFICATION_LIST", 0x05: "PORTFOLIO_UPDATE",
    0x06: "SYSTEM_UPDATE", 0x07: "CALENDAR_UPDATE", 0x08: "CONFIG",
    0x09: "REBOOT", 0x0A: "FACTORY_RESET",
}

FRENCH_DAYS = ["lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi", "dimanche"]
FRENCH_MONTHS = [
    "janvier", "février", "mars", "avril", "mai", "juin",
    "juillet", "août", "septembre", "octobre", "novembre", "décembre"
]

PROTO_MAJOR = 1
PROTO_MINOR = 0

# =============================================================================
# CRC-16/CCITT
# =============================================================================

def crc16_ccitt(data: bytes, crc: int = 0x0000) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

# =============================================================================
# Frame encode / decode
# =============================================================================

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
    """Incremental frame decoder matching the C++ FrameDecoder."""

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
            else:
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
            yield ("crc_error", msg_type, payload)
        else:
            yield ("frame", msg_type, payload)


# =============================================================================
# Emulator
# =============================================================================

class Esp32Emulator:
    def __init__(self, port: str, baud: int, log_path: str | None = None):
        self.port = port
        self.baud = baud
        self.ser: serial.Serial | None = None
        self.decoder = FrameDecoder()
        self.hb_sequence = 0
        self.last_hb_sent = 0.0
        self.last_hb_received = 0.0
        self.running = False
        self.connected = False

        # Current time state (updated from TIME messages)
        self.current_time: dict | None = None

        # Logging
        self.log_file = None
        if log_path:
            self.log_file = open(log_path, "a", encoding="utf-8")

    def log(self, msg: str):
        """Log with timestamp to console and file."""
        ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        line = f"[{ts}] {msg}"
        print(line, flush=True)
        if self.log_file:
            self.log_file.write(line + "\n")
            self.log_file.flush()

    def send(self, msg_type: int, payload: bytes = b""):
        frame = encode(msg_type, payload)
        self.ser.write(frame)
        name = TYPE_NAMES.get(msg_type, f"0x{msg_type:02X}")
        detail = self._describe_payload(msg_type, payload)
        self.log(f"--> {name} ({len(payload)}B) {detail}  raw={frame.hex()}")

    def send_heartbeat(self):
        payload = bytes([self.hb_sequence & 0xFF, PROTO_MAJOR, PROTO_MINOR])
        self.hb_sequence += 1
        self.send(HEARTBEAT, payload)
        self.last_hb_sent = time.monotonic()

    def send_ack(self, acked_type: int):
        self.send(ACK, bytes([acked_type, 0]))

    def send_nack(self, nacked_type: int, error_code: int):
        self.send(NACK, bytes([nacked_type, error_code, 0]))

    def send_time_request(self):
        self.send(REQUEST, bytes([0x01]))  # 0x01 = TIME_SYNC

    def send_touch(self, event: int, x: int, y: int):
        payload = bytes([event, 1]) + x.to_bytes(2, "big") + y.to_bytes(2, "big") + bytes([128]) + int(time.time() * 1000).to_bytes(4, "big")
        self.send(TOUCH, payload)

    def _describe_payload(self, msg_type: int, payload: bytes) -> str:
        if msg_type == TIME and len(payload) >= 19:
            # New format: uint64 timestamp_ms (LE) + int16 tz_min (LE) + uint8 dst
            #             + hour + minute + second + day + month + uint16 year (LE) + dow
            ts_ms = int.from_bytes(payload[0:8], "little")
            tz_min = int.from_bytes(payload[8:10], "little", signed=True)
            dst = payload[10]
            h, m, s, d, mo = payload[11], payload[12], payload[13], payload[14], payload[15]
            yr = int.from_bytes(payload[16:18], "little")
            dow = payload[18] if len(payload) > 18 else 0
            day_name = FRENCH_DAYS[dow] if 0 <= dow < 7 else "?"
            month_name = FRENCH_MONTHS[mo - 1] if 1 <= mo <= 12 else "?"
            # Show UTC time from timestamp for verification
            utc_dt = datetime.datetime.fromtimestamp(ts_ms / 1000.0, tz=datetime.timezone.utc)
            return (f"{h:02d}:{m:02d}:{s:02d} {day_name} {d} {month_name} {yr} "
                    f"(ts_ms={ts_ms} utc={utc_dt.strftime('%H:%M:%S')} tz={tz_min:+d} dst={dst})")
        if msg_type == WEATHER and len(payload) >= 28:
            ts_ms = int.from_bytes(payload[0:8], "little")
            temp = int.from_bytes(payload[8:10], "little", signed=True) / 10.0
            feels = int.from_bytes(payload[10:12], "little", signed=True) / 10.0
            rh = payload[12]
            cond = payload[13]
            uv = payload[14]
            aqi = int.from_bytes(payload[15:17], "little")
            wind_spd = int.from_bytes(payload[17:19], "little") / 10.0
            wind_dir = int.from_bytes(payload[19:21], "little")
            tmr_min = int.from_bytes(payload[21:23], "little", signed=True) / 10.0
            tmr_max = int.from_bytes(payload[23:25], "little", signed=True) / 10.0
            tmr_cond = payload[25]
            pressure = int.from_bytes(payload[26:28], "little") / 10.0
            return (f"temp={temp:.1f}C feels={feels:.1f}C RH={rh}% cond={cond} UV={uv} "
                    f"AQI={aqi} wind={wind_spd:.1f}m/s@{wind_dir} "
                    f"tmr={tmr_min:.1f}/{tmr_max:.1f}C cond={tmr_cond} P={pressure:.1f}hPa")
        if msg_type == MEDIA and len(payload) >= 19:
            ts_ms = int.from_bytes(payload[0:8], "little")
            pos_ms = int.from_bytes(payload[8:12], "little")
            dur_ms = int.from_bytes(payload[12:16], "little")
            state = payload[16]
            tlen = payload[17]
            alen = payload[18]
            title = payload[19:19+tlen].decode("utf-8", errors="replace")
            artist = payload[19+tlen:19+tlen+alen].decode("utf-8", errors="replace")
            state_name = {0: "STOPPED", 1: "PAUSED", 2: "PLAYING"}.get(state, "?")
            pos_str = f"{pos_ms//60000}:{pos_ms//1000%60:02d}"
            dur_str = f"{dur_ms//60000}:{dur_ms//1000%60:02d}" if dur_ms else "-:--"
            return f"state={state_name} {pos_str}/{dur_str} \"{title}\" — \"{artist}\""
        if msg_type == SYSTEM and len(payload) >= 22:
            ts_ms = int.from_bytes(payload[0:8], "little")
            cpu = int.from_bytes(payload[8:10], "little") / 10.0
            gpu = int.from_bytes(payload[10:12], "little") / 10.0
            ram_used = int.from_bytes(payload[12:14], "little")
            ram_total = int.from_bytes(payload[14:16], "little")
            net_up = int.from_bytes(payload[16:20], "little")
            net_down = int.from_bytes(payload[20:24], "little")
            return (f"CPU={cpu:.1f}% GPU={gpu:.1f}% RAM={ram_used}/{ram_total}MB "
                    f"NET={net_down}/{net_up}KB/s down/up")
        if msg_type == HEARTBEAT and len(payload) >= 3:
            return f"seq={payload[0]} proto={payload[1]}.{payload[2]}"
        if msg_type == ACK and len(payload) >= 1:
            return f"acked={TYPE_NAMES.get(payload[0], hex(payload[0]))}"
        if msg_type == NACK and len(payload) >= 2:
            return f"nacked={TYPE_NAMES.get(payload[0], hex(payload[0]))} err={ERROR_NAMES.get(payload[1], hex(payload[1]))}"
        if msg_type == REQUEST and len(payload) >= 1:
            return f"req={REQUEST_NAMES.get(payload[0], hex(payload[0]))}"
        if payload:
            return f"hex={payload[:32].hex()}{'...' if len(payload) > 32 else ''}"
        return ""

    def handle_frame(self, msg_type: int, payload: bytes):
        name = TYPE_NAMES.get(msg_type, f"0x{msg_type:02X}")
        detail = self._describe_payload(msg_type, payload)
        self.log(f"<-- {name} ({len(payload)}B) {detail}")

        if msg_type == HEARTBEAT:
            self.last_hb_received = time.monotonic()
            if not self.connected:
                self.connected = True
                self.log("*** CONNECTION ESTABLISHED (server heartbeat received) ***")
                # Request initial time sync
                self.send_time_request()
            # Respond to server heartbeat (per protocol: ESP32 responds within 100ms)
            self.send_heartbeat()

        elif msg_type == TIME:
            if len(payload) >= 19:
                ts_ms = int.from_bytes(payload[0:8], "little")
                tz_min = int.from_bytes(payload[8:10], "little", signed=True)
                self.current_time = {
                    "timestamp_ms": ts_ms,
                    "tz_min": tz_min,
                    "hour": payload[11], "minute": payload[12], "second": payload[13],
                    "day": payload[14], "month": payload[15],
                    "year": int.from_bytes(payload[16:18], "little"),
                    "dow": payload[18] if len(payload) > 18 else 0,
                }
                t = self.current_time
                day_name = FRENCH_DAYS[t["dow"]] if 0 <= t["dow"] < 7 else "?"
                month_name = FRENCH_MONTHS[t["month"] - 1] if 1 <= t["month"] <= 12 else "?"
                utc_dt = datetime.datetime.fromtimestamp(ts_ms / 1000.0, tz=datetime.timezone.utc)
                self.log(f"    TIME SYNCED: {t['hour']:02d}:{t['minute']:02d}:{t['second']:02d} "
                         f"{day_name} {t['day']} {month_name} {t['year']} "
                         f"(utc={utc_dt.strftime('%H:%M:%S')} ts_ms={ts_ms})")
            self.send_ack(TIME)

        elif msg_type == DISCOVER:
            # Respond with device identification for auto-discovery
            self.send(DISCOVER_ACK, DISCOVER_ACK_PAYLOAD)

        else:
            # ACK any other valid frame
            self.send_ack(msg_type)

    def handle_crc_error(self, msg_type: int):
        self.log(f"<-- CRC ERROR on type=0x{msg_type:02X} -> NACK")
        self.send_nack(msg_type, 0x02)

    def run(self, duration: float | None = None):
        self.ser = serial.Serial(self.port, self.baud, timeout=0.01)  # 10ms poll
        self.running = True
        self.last_hb_sent = time.monotonic()
        self.last_hb_received = time.monotonic()
        self.log(f"=== ESP32 Emulator started on {self.port} @ {self.baud} baud ===")
        self.log(f"Protocol v{PROTO_MAJOR}.{PROTO_MINOR}, waiting for server...")

        # Send initial heartbeat to announce presence
        self.send_heartbeat()

        start = time.monotonic()
        try:
            while self.running:
                if duration and (time.monotonic() - start) > duration:
                    self.log(f"Duration limit ({duration}s) reached, stopping.")
                    break

                # Read available data
                data = self.ser.read(self.ser.in_waiting or 1)
                if data:
                    for result in self.decoder.feed(data):
                        if result[0] == "frame":
                            self.handle_frame(result[1], result[2])
                        elif result[0] == "crc_error":
                            self.handle_crc_error(result[1])

                # Send periodic heartbeat (every 5s) — acts as keep-alive from ESP32 side too
                now = time.monotonic()
                if now - self.last_hb_sent >= 5.0:
                    self.send_heartbeat()

                # Check connection timeout (15s without server heartbeat)
                if self.connected and (now - self.last_hb_received) > 15.0:
                    self.connected = False
                    self.log("*** CONNECTION LOST (no server heartbeat for 15s) ***")

        except KeyboardInterrupt:
            self.log("Interrupted by user (Ctrl+C)")
        finally:
            self.running = False
            self.ser.close()
            self.log("=== ESP32 Emulator stopped ===")
            if self.log_file:
                self.log_file.close()


def main() -> int:
    ap = argparse.ArgumentParser(description="ESP32 emulator for LeDashboard server testing")
    ap.add_argument("--port", required=True, help="COM port (e.g. COM6)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--log", default=None, help="Log file path (default: emulator_<port>.log)")
    ap.add_argument("--duration", type=float, default=None, help="Run for N seconds then stop")
    args = ap.parse_args()

    log_path = args.log
    if log_path is None:
        log_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            f"emulator_{args.port.replace(':', '')}.log"
        )

    emu = Esp32Emulator(args.port, args.baud, log_path)
    emu.run(duration=args.duration)
    return 0


if __name__ == "__main__":
    sys.exit(main())
