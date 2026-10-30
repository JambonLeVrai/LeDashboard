# Server/Host C++ Software

C++ host application that collects data from various sources (time, weather,
media player, notifications, portfolio, system monitor) and sends it to the
ESP32 dashboard over USB serial.

## Layout

```
server/
├── CMakeLists.txt        # build config, wired to vcpkg packages
├── CMakePresets.json     # windows-msvc (build/) and windows-ninja (build-ninja/)
├── vcpkg.json            # dependency manifest (nlohmann-json, spdlog, cpr, lz4, doctest)
├── src/                  # sources (main.cpp entry point)
├── include/              # headers
└── tools/
    ├── mock_esp32.py     # pyserial mock of the ESP32 for hardware-less testing
    └── setup_com0com.ps1 # elevated: installs com0com and creates COM20 <-> COM21
```

## Build

Requires the configured toolchain (see `../docs/context/DEV_ENVIRONMENT.md`):
Visual Studio 2022 (C++ workload), CMake, Ninja, and vcpkg with `VCPKG_ROOT` set.
Dependencies are installed automatically by vcpkg manifest mode on configure.

```powershell
cd server
cmake --preset windows-ninja        # or: cmake --preset windows-msvc
cmake --build --preset windows-ninja
# binary: build-ninja\bin\ledashboard_server.exe
```

## Testing without hardware

1. Create the virtual COM pair once, from an elevated PowerShell:
   `powershell -ExecutionPolicy Bypass -File tools\setup_com0com.ps1`
   (com0com is test-signed: on x64 it needs `bcdedit -set testsigning ON` + reboot)
2. Start the mock ESP32 on one end of the pair, using the project conda env only:
   `C:\ProgramData\anaconda3\envs\le_dashboard_env\python.exe tools\mock_esp32.py --port COM21`
3. Point the server at the other end (`COM20`).

## Status

- Scaffold in place: CMakeLists.txt, CMakePresets.json, vcpkg.json, stub `src/main.cpp`.
- Not yet implemented: serial port handling, protocol encode/decode, data sources.
