# LeDashboard

A custom dashboard system with ESP32 firmware and C++ host application, displaying time, weather, media player, notifications, portfolio, system monitor, and calendar information on a 480x480 TFT display.

## Project Structure

```
LeDashboard/
├── docs/                      # Documentation
│   └── context/              # Context and specifications
│       ├── COMMUNICATION_PROTOCOL.md
│       ├── FEATURES.md
│       └── ...
├── firmware/                  # ESP32 Firmware
│   ├── include/              # Header files
│   │   ├── protocol.h        # Communication protocol definitions
│   │   ├── data/             # Data structure headers
│   │   └── ui/               # UI component headers
│   ├── src/                 # Source files
│   │   ├── data/             # Data implementations (dummy for now)
│   │   └── ui/               # UI implementations
│   ├── test/                 # Test files
│   ├── platformio.ini        # PlatformIO configuration
│   └── README.md
├── server/                    # C++ Host Application
│   ├── include/
│   ├── src/
│   ├── CMakeLists.txt
│   └── README.md
└── README.md                 # This file
```

## Current Status

### ✅ Implemented
- **Project Structure**: Reorganized into firmware/ and server/ folders
- **Protocol Constants**: Added French localization, format constants, CRC functions
- **Data Structures**: Time and weather data structures with French formatting
- **UI Framework**: LVGL-based page system with base classes
- **Time+Weather Page**: First page implementation with French date/time, weather display
- **Dummy Data**: Hardcoded test data for development and testing

### 🔄 In Progress
- **LVGL Integration**: Basic display and touch setup
- **Time Updates**: Simulated time progression for testing

### ⏭️ Planned
- **Page Navigation**: Swipe between pages (portfolio, system, calendar)
- **Media Player Overlay**: Display at bottom of screen
- **Real Serial Communication**: Protocol implementation with host
- **Notifications**: Popup notification system
- **Additional Pages**: Portfolio, system monitor, calendar

## Key Features

### Display & UI
- **480x480 TFT Display** with touch support (GT911)
- **LVGL Graphics Library** for rich, responsive UI
- **French Localization**: Date, time, and weather in French
- **24-hour Format**: Default time display
- **Page-based Navigation**: Swipe left/right to switch pages

### Data Display
1. **Time & Weather Page** (Default):
   - Date: "Jeudi 24 octobre 2026"
   - Time: "13:40" (large)
   - Current weather with icon
   - Temperature, Humidity (RH), Air Quality, UV Index
   - Tomorrow's forecast with min/max temperatures

2. **Media Player**: Overlay at bottom, expandable on touch
3. **Notifications**: Popups on current interface
4. **Portfolio**: Financial data display
5. **System Monitor**: CPU, memory, storage, network, GPU metrics
6. **Calendar**: Upcoming events

### Communication Protocol
- **Bidirectional Serial**: Host ↔ ESP32
- **Custom Binary Protocol** with message framing
- **CRC-16/CCITT** checksum for error detection
- **Byte Stuffing** for special characters (SOF, EOF, ESC)
- **Configurable Format**: JSON (debug) or Binary (production)
- **Compression**: LZ4 for large payloads
- **Baud Rate**: 115200 (default, configurable)

## Development Setup

### ESP32 Firmware

1. **Install PlatformIO**:
   ```bash
   pio install
   ```

2. **Configure platformio.ini**:
   - Set correct display type and pins
   - Configure serial port
   - Enable PSRAM if available

3. **Build and Upload**:
   ```bash
   pio run -t upload
   ```

### Host Application

Toolchain and env vars are configured as documented in [docs/context/DEV_ENVIRONMENT.md](docs/context/DEV_ENVIRONMENT.md).

1. **CMake Build** (deps from `server/vcpkg.json` auto-install via vcpkg):
   ```bash
   cd server
   cmake --preset windows-ninja
   cmake --build --preset windows-ninja
   ```

2. **Run Server**:
   ```bash
   .\build-ninja\bin\ledashboard_server.exe
   ```

## Hardware Requirements

### Display
- **ESP32-4848S040**: 480x480 TFT display with touch
- **Driver**: ST7796
- **Touch Controller**: GT911
- **Interface**: SPI

### Wiring
```
ESP32          Display
+------------------+
| GPIO23 |----->| SCLK  |
| GPIO19 |----->| MISO  |
| GPIO22 |----->| MOSI  |
| GPIO5  |----->| CS    |
| GPIO21 |----->| DC    |
| GPIO18 |----->| RST   |
| GPIO19 |----->| BL    |
+------------------+

ESP32          GT911 Touch
+------------------+
| GPIO12 |----->| INT   |
| GPIO13 |----->| SDA   |
| GPIO14 |----->| SCL   |
| GPIO15 |----->| RST   |
+------------------+

Host PC        ESP32
+------------------+
| TX    |----->| RX (GPIO3) |
| RX    |----->| TX (GPIO1) |
| GND   |----->| GND        |
+------------------+
```

## Configuration

### Format Selection
Edit `firmware/include/protocol.h`:
```cpp
// Change between JSON and Binary formats
#define DATA_FORMAT USE_JSON_FORMAT   // or USE_BINARY_FORMAT
```

### Serial Settings
- **Baud Rate**: Configurable (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)
- **Default**: 115200

### Display Settings
- **Brightness**: Configurable
- **Timeout**: Screen off after inactivity
- **Orientation**: Portrait/Landscape
- **Theme**: Light/Dark/Custom

## Dependencies

### ESP32 Firmware
- PlatformIO
- LVGL v8.3.0+
- TFT_eSPI v2.5.0+
- ESP32 Arduino Core

### Host Application
- C++17 compiler (MSVC via Visual Studio 2022)
- CMake 3.15+ (3.29.3) + Ninja 1.13.2
- vcpkg (`C:\vcpkg`, `VCPKG_ROOT` set, triplet `x64-windows`) managing: nlohmann-json, spdlog, cpr, lz4, doctest — see `server/vcpkg.json`

## License

This project is licensed under the MIT License.

## Contributing

Contributions are welcome! Please follow these guidelines:

1. **Code Style**: Consistent with existing code
2. **Documentation**: Update relevant docs
3. **Testing**: Test changes thoroughly
4. **Commits**: Use clear, descriptive messages

## Support

For issues, questions, or feature requests, please open an issue on the project repository.

---

*LeDashboard - Custom Dashboard System for ESP32 and C++ Host*
*Version: 1.0.0*
*Last Updated: 2025-01-08*
