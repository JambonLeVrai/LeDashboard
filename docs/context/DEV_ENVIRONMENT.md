# Development Environment (as configured)

This file records the actual, verified development setup for this project on the
dev machine (Windows, user `kimer`, Git Bash shell). When something here changes,
update this file in the same commit.

Last verified: 2026-10-30

---

## 1. Toolchain

| Tool | Version | Location / notes |
|------|---------|------------------|
| Visual Studio 2022 Community | MSVC 14.40.33807 | `C:\Program Files\Microsoft Visual Studio\2022\Community` — "Desktop development with C++" workload, includes Windows 10 SDK (`Setupapi.lib` used by the server) |
| CMake | 3.29.3 | `C:\Program Files\CMake\bin` (on PATH) |
| Ninja | 1.13.2 | via chocolatey, `C:\ProgramData\chocolatey\bin\ninja` (on PATH) |
| Git | 2.45.1.windows.1 | `/mingw64/bin/git` |
| vcpkg | 2026-09-26-51bf87ca | `C:\vcpkg`, bootstrapped (`vcpkg.exe` present) |
| Python (project) | conda env `le_dashboard_env` | `C:\ProgramData\anaconda3\envs\le_dashboard_env` — **the only Python to use in this project** |
| com0com | files in `C:\Program Files (x86)\com0com` | virtual null-modem; driver install pending (see §5) |

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
├── src/main.cpp          # entry point
└── tools/
    ├── mock_esp32.py         # pyserial mock of the ESP32 (protocol: SOF/EOF/CRC16/stuffing)
    └── setup_com0com.ps1     # elevated script: installs com0com + creates COM20<->COM21
```

Both presets set `CMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
and `VCPKG_TARGET_TRIPLET=x64-windows`, so **dependencies from `vcpkg.json` install
automatically on configure** (manifest mode, into `server/vcpkg_installed/`).

Commands (from `server/`):

```powershell
cmake --preset windows-ninja        # configure + auto-install deps
cmake --build --preset windows-ninja
.\build-ninja\bin\ledashboard_server.exe

# or with the Visual Studio generator:
cmake --preset windows-msvc
cmake --build --preset windows-msvc    # builds Release
```

Manual equivalent (no presets):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Classic (non-manifest) vcpkg usage also works since `VCPKG_ROOT`/`PATH` are set:
`vcpkg install <pkg>` (installs to `C:\vcpkg\installed\x64-windows`).

## 4. Python for this project

- Use **only** the conda env `le_dashboard_env`
  (`C:\ProgramData\anaconda3\envs\le_dashboard_env`), e.g.:
  `conda run -n le_dashboard_env python ...` or the env's `python.exe` by full path.
- Project Python packages: `pyserial` (for `server/tools/mock_esp32.py`).
- Note: the env directory lives under `C:\ProgramData` and is writable only by
  Administrators, so `conda install -n le_dashboard_env ...` must be run from an
  **elevated** Anaconda Prompt.

## 5. Serial testing without hardware

Physical ports present: `COM1`, `COM3` (M2k Serial Console — the ESP32 board),
`COM4` (USB-SERIAL CH340).

For hardware-less tests, com0com provides a virtual null-modem pair
**COM20 <-> COM21**:

1. Run once, elevated:
   `powershell -ExecutionPolicy Bypass -File server\tools\setup_com0com.ps1 [-EnableTestSigning]`
   - com0com is a test-signed driver: on x64 it only loads with
     `bcdedit -set testsigning ON` + reboot (the script checks this and can enable it).
2. Start the mock ESP32 on one end (project conda env):
   `C:\ProgramData\anaconda3\envs\le_dashboard_env\python.exe server\tools\mock_esp32.py --port COM21`
3. Point the server at the other end (`COM20`).

Protocol implemented by the mock (see COMMUNICATION_PROTOCOL.md): frame
`SOF(0xAA) | Type(1) | Length(2, BE) | Payload(N) | CRC16-CCITT(2, BE) | EOF(0x55)`,
CRC over Type+Length+Payload, byte stuffing ESC(0x5A)+XOR(0xA5) applied to the whole
frame body (the reference decoder unstuffs every byte between SOF and EOF).
The mock ACKs valid frames, answers HEARTBEAT, NACKs CRC errors.

## 6. Notes / gotchas

- New environment variables require a **new shell** to take effect.
- First configure of `server/` builds cpr/curl/zlib/fmt from source (vcpkg binary
  cache push needs auth) — expect several minutes on first run.
- `server/build/`, `server/build-ninja/`, `server/vcpkg_installed/` are gitignored.
- Do not commit secrets (API keys) into `server/config/`.
