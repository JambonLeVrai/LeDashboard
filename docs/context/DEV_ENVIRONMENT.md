# Development Environment (as configured)

This file records the actual, verified development setup for this project on the
dev machine (Windows, user `kimer`, Git Bash shell). When something here changes,
update this file in the same commit.

Last verified: 2026-11-01

---

## Project status (v1.3.0, working)

| Feature | Status | Details |
|---------|--------|---------|
| Heartbeat | Working | 5s interval, connection state tracking |
| Time sync | Working | ms-precision via `timestamp_ms`, instant on start/connect |
| Weather | Working | Open-Meteo (no key), hourly, current+tomorrow+AQI |
| Media | Working | Windows SMTC, title/artist/timer/state, 1s refresh |
| System monitor | Protocol done, firmware UI done (dummy values) | SYSTEM message every 5s (CPU/RAM/NET); GPU via PDH TODO |
| Auto-discovery | Working | `--port auto` scans COM ports with DISCOVER probe |
| Pages | Working | Page 1 = time+weather+media, Page 2 = system charts; double-tap to switch |

---

## 1. Toolchain

| Tool | Version | Location / notes |
|------|---------|------------------|
| Visual Studio 2022 Community | MSVC 14.40.33807 | `C:\Program Files\Microsoft Visual Studio\2022\Community` — "Desktop development with C++" workload, includes Windows 10 SDK (`Setupapi.lib`, C++/WinRT headers for SMTC) |
| CMake | 3.29.3 | `C:\Program Files\CMake\bin` (on PATH) |
| Ninja | 1.13.2 | via chocolatey, `C:\ProgramData\chocolatey\bin\ninja` (on PATH) |
| Git | 2.45.1.windows.1 | `/mingw64/bin/git` |
| vcpkg | 2026-09-26-51bf87ca | `C:\vcpkg`, bootstrapped (`vcpkg.exe` present) |
| Python (project) | conda env `le_dashboard_env` | `C:\ProgramData\anaconda3\envs\le_dashboard_env` — **the only Python to use in this project** |
| com0com | files in `C:\Program Files (x86)\com0com` | virtual null-modem; installed via choco; pair **COM5 <-> COM6** active (CNCA0/CNCB0) |

MinGW g++ also exists (`C:\MinGW`) but is **not** used for this project — vcpkg
triplet `x64-windows` targets MSVC.

## 2. vcpkg configuration

Set as **user environment variables** (visible in new shells; run
`refreshenv` or open a new terminal after changing them):

| Variable | Value |
|----------|-------|
| `VCPKG_ROOT` | `C:\vcpkg` |
| `PATH` | appended `C:\vcpkg` |
| `VCPKG_DEFAULT_TRIPLET` | `x64-windows` |
| `VCPKG_BINARY_SOURCES` | `clear;nuget,https://nuget.pwsh.io/packages,readwrite` (binary caching; pushing needs auth, reading falls back to source builds) |

Smoke test passed: `vcpkg install lz4` (x64-windows) completed successfully.

## 3. Server build (C++)

Layout:

```
server/
├── CMakeLists.txt        # wired to vcpkg packages via find_package(... CONFIG REQUIRED)
├── CMakePresets.json     # presets: windows-msvc (build/), windows-ninja (build-ninja/)
├── vcpkg.json            # manifest: nlohmann-json, spdlog, cpr, lz4, doctest
├── include/              # protocol.h, serial_port.h, server.h, weather_source.h, media_source.h
├── src/
│   ├── main.cpp          # entry point (--port, --baud, --city, --log-level, --config)
│   ├── protocol.cpp      # frame encode/decode (SOF/EOF/CRC16/byte-stuffing)
│   ├── serial_port.cpp   # COM port with overlapped I/O, read+write threads, bounded write queue
│   ├── server.cpp        # main loop: TIME 1s, MEDIA 1s, HB 5s, WEATHER on change
│   ├── weather_source.cpp# Open-Meteo fetch (background thread, hourly)
│   └── media_source.cpp  # Windows SMTC (background thread, 200ms poll, position tracking)
└── tools/
    ├── test_esp32_emulator.py # full ESP32 emulator with timestamped logging
    ├── mock_esp32.py          # simple mock (legacy)
    └── setup_com0com.ps1      # elevated script for com0com setup
```

Both presets set `CMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
and `VCPKG_TARGET_TRIPLET=x64-windows`, so **dependencies from `vcpkg.json` install
automatically on configure** (manifest mode, into `server/vcpkg_installed/`).

Commands (from `server/`):

```powershell
cmake --preset windows-ninja        # configure + auto-install deps
cmake --build --preset windows-ninja
.\build-ninja\bin\ledashboard_server.exe --port COM4 --city Paris
# --log-level debug shows every MEDIA/TIME/WEATHER frame with M:SS/M:SS timers
```

### Running the server

```
ledashboard_server.exe [--port COMx] [--baud N] [--city "City"] [--log-level LEVEL] [--config file.json]
```

| Arg | Default | Description |
|-----|---------|-------------|
| `--port` | COM3 | Serial port (use COM4 for ESP32 CH340, COM5 for com0com testing) |
| `--baud` | 115200 | Baud rate |
| `--city` | Paris | Weather location (city name or "lat,lon"), Open-Meteo, no API key |
| `--log-level` | info | trace/debug/info/warn/error/off — debug shows per-frame MEDIA timers |
| `--config` | — | JSON config file path |

### What works (verified 2026-11-01)

- **Heartbeat**: server sends every 5s, ESP32 echoes; connection state tracked (CONNECTED/LOST)
- **Time sync**: ms-precision via `uint64_t timestamp_ms` in TIME message; ESP32 anchors `millis()` at reception; instant sync on server start and on connect
- **Weather**: Open-Meteo (no key), geocoding + current + tomorrow + AQI; sent on connect and hourly; firmware updates icons/labels only on change
- **Media**: Windows SMTC (works with Spotify, Vivaldi/YouTube, any app); title+artist+timer+play/pause; server tracks position (snaps to SMTC when app updates, advances by wall-clock otherwise); state changes sent immediately via SMTC events
- **Serial I/O**: overlapped (async) I/O on COM port, dedicated read+write threads, bounded write queue — no blocking, no deadlocks
- **ESP32 firmware**: non-blocking serial reads, change-detected LVGL updates, fixed (non-scrollable) UI, static frame buffers (no stack overflow)

## 4. Python for this project

- Use **only** the conda env `le_dashboard_env`
  (`C:\ProgramData\anaconda3\envs\le_dashboard_env`), e.g.:
  `conda run -n le_dashboard_env python ...` or the env's `python.exe` by full path.
- Project Python packages: `pyserial` (for `server/tools/test_esp32_emulator.py`).
- Note: the env directory lives under `C:\ProgramData` and is writable only by
  Administrators, so `conda install -n le_dashboard_env ...` must be run from an
  **elevated** Anaconda Prompt.

## 5. Serial testing without hardware

Physical ports present: `COM1`, `COM3` (M2k Serial Console), `COM4` (USB-SERIAL CH340 — the ESP32 board).

For hardware-less tests, com0com provides a virtual null-modem pair **COM5 <-> COM6**
(installed via `choco install com0com`, ports CNCA0/CNCB0 renamed to COM5/COM6):

1. Start the emulator on one end (project conda env):
   `C:\ProgramData\anaconda3\envs\le_dashboard_env\python.exe server\tools\test_esp32_emulator.py --port COM6`
2. Point the server at the other end: `ledashboard_server.exe --port COM5`

Protocol: frame `SOF(0xAA) | Type(1) | Length(2, BE) | Payload(N) | CRC16-CCITT(2, BE) | EOF(0x55)`,
CRC over Type+Length+Payload, byte stuffing ESC(0x5A)+XOR(0xA5) applied to the whole
frame body. See COMMUNICATION_PROTOCOL.md for full message specs.

## 6. Notes / gotchas

- New environment variables require a **new shell** to take effect.
- First configure of `server/` builds cpr/curl/zlib/fmt from source (vcpkg binary
  cache push needs auth) — expect several minutes on first run.
- `server/build/`, `server/build-ninja/`, `server/vcpkg_installed/` are gitignored.
- Do not commit secrets (API keys) into `server/config/`.
- com0com on x64 needs test signing (`bcdedit -set testsigning on` + reboot) — already done on this machine.
- ESP32 firmware: `Serial` (USB CDC) is shared between debug prints and protocol frames. The decoder scans for SOF (0xAA) and ignores non-frame bytes, so debug text is harmless in practice.
