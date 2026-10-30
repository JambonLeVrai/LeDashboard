# ESP32 Dashboard Project - Context Documentation

## Overview

This directory contains **comprehensive context documentation** for the ESP32 Dashboard project. Use these files as reference in subsequent sessions with Vibe to maintain continuity and avoid repeating information.

---

## Project Description

**Hardware**: ESP32-4848S040 board with integrated 480x480 touchscreen display

**Architecture**: Two-component system
1. **C++ Server Application** - Runs on the host computer
2. **ESP32 Firmware** - Runs on the microcontroller board

**Communication**: USB Serial (115200 baud by default)

**UI Framework**: LVGL (Light and Versatile Graphics Library)

**Build System**: PlatformIO for ESP32, CMake for C++ server

---

## Documentation Index

### 📋 Core Project Documentation

| File | Description | Scope |
|------|-------------|-------|
| [PROJECT_OVERVIEW.md](./PROJECT_OVERVIEW.md) | High-level architecture, tech stack, and project goals | Full project |
| [FEATURES.md](./FEATURES.md) | Detailed specifications for all dashboard features | Features |
| [TECHNICAL_SPECIFICATIONS.md](./TECHNICAL_SPECIFICATIONS.md) | Hardware, software, and performance specifications | Technical |
| [IMPLEMENTATION_GUIDE.md](./IMPLEMENTATION_GUIDE.md) | Step-by-step implementation instructions | Development |
| [DEV_ENVIRONMENT.md](./DEV_ENVIRONMENT.md) | Configured toolchain, env vars, build presets, test tooling | Development |

### 🔄 Communication Layer

| File | Description |
|------|-------------|
| [COMMUNICATION_PROTOCOL.md](./COMMUNICATION_PROTOCOL.md) | Serial protocol specification, message formats, encoding |
| [API_INTEGRATIONS.md](./API_INTEGRATIONS.md) | External APIs and service integrations (Weather, Media, Thunderbird, Portfolio) |

### 🎨 User Interface

| File | Description |
|------|-------------|
| [UI_DESIGN.md](./UI_DESIGN.md) | Overall UI design, layout, color scheme, typography |
| [UI_WIDGETS.md](./UI_WIDGETS.md) | Detailed widget implementations and code examples |
| [UI_NAVIGATION.md](./UI_NAVIGATION.md) | Screen management, navigation, gestures, transitions |
| [UI_THEMES.md](./UI_THEMES.md) | Theming system, color palettes, typography, customization |
| [UI_ANIMATIONS.md](./UI_ANIMATIONS.md) | Animation system, easing functions, transition effects |

---

## Quick Reference

### Project Structure

```
LeDashboard/
├── docs/
│   └── context/                    # ← This directory
│       ├── PROJECT_OVERVIEW.md     # Project architecture
│       ├── FEATURES.md             # Feature specifications
│       ├── TECHNICAL_SPECIFICATIONS.md # Hardware/software specs
│       ├── COMMUNICATION_PROTOCOL.md # Serial protocol
│       ├── API_INTEGRATIONS.md      # API integrations
│       ├── UI_DESIGN.md             # UI design
│       ├── UI_WIDGETS.md            # Widget implementations
│       ├── UI_NAVIGATION.md         # Navigation system
│       ├── UI_THEMES.md             # Theming system
│       ├── UI_ANIMATIONS.md         # Animation system
│       └── IMPLEMENTATION_GUIDE.md   # Implementation guide
│
├── server/                       # C++ Server Application
│   ├── CMakeLists.txt
│   ├── main.cpp
│   └── ...
│
└── firmware/                      # ESP32 Firmware (PlatformIO)
    ├── platformio.ini
    ├── src/
    │   ├── main.cpp
    │   └── ...
    └── lib/
```

### Feature List

| Feature | Description | Data Source |
|---------|-------------|-------------|
| **Time** | Current time and date | System clock |
| **Weather** | Current and forecast weather | OpenWeatherMap API |
| **Media** | Currently playing media with progress | Media player APIs (Spotify, Winamp, VLC) |
| **Notifications** | Email and app notifications | Thunderbird mailbox, Windows notifications |
| **Portfolio** | Stock portfolio statistics | Self-hosted API |
| **System Monitor** | CPU, memory, network, battery | System APIs |

### Hardware Specifications

- **MCU**: ESP32 dual-core Tensilica Xtensa LX6
- **Display**: 480x480 pixels, 16-bit color (65,536 colors)
- **Touch**: Capacitive or resistive touch panel
- **Memory**: 520 KB SRAM, external PSRAM available
- **Flash**: 4 MB (or more)
- **Connectivity**: USB, WiFi, Bluetooth

### Software Stack

#### Server Side (C++)
- **Build**: CMake presets (`server/CMakePresets.json`) + Visual Studio 2022 / Ninja
- **Dependencies**: vcpkg manifest mode (`server/vcpkg.json`) at `C:\vcpkg`
- **HTTP**: cpr (C++ Requests)
- **JSON**: nlohmann/json
- **Logging**: spdlog
- **Compression**: LZ4
- **Tests**: doctest
- **Serial**: Windows API (Setupapi) / POSIX; com0com virtual pair for hardware-less tests

#### ESP32 Side (C++ with Arduino)
- **Framework**: PlatformIO + Arduino
- **Graphics**: LVGL 8.3+
- **Display Driver**: LovyanGFX or TFT_eSPI
- **Touch Driver**: XPT2046 or similar

### Communication Protocol

- **Physical**: USB Serial
- **Baud Rate**: 115200 (configurable)
- **Protocol**: Custom frame-based with SOF/EOF markers
- **Encoding**: Byte stuffing for special bytes
- **Error Detection**: CRC-16
- **Message Types**: HEARTBEAT, TIME, WEATHER, MEDIA, NOTIFICATION, PORTFOLIO, SYSTEM, etc.

---

## How to Use This Documentation

### For New Sessions

1. **Start with PROJECT_OVERVIEW.md** - Understand the overall architecture
2. **Review FEATURES.md** - See what features are implemented/planned
3. **Check TECHNICAL_SPECIFICATIONS.md** - Hardware and software constraints
4. **Read COMMUNICATION_PROTOCOL.md** - Understand how components communicate
5. **Consult API_INTEGRATIONS.md** - For external data source details
6. **Use UI documentation** - For UI/UX implementation

### For Specific Tasks

| Task | Relevant Files |
|------|---------------|
| Adding a new feature | FEATURES.md, IMPLEMENTATION_GUIDE.md |
| Fixing communication issues | COMMUNICATION_PROTOCOL.md, TECHNICAL_SPECIFICATIONS.md |
| Creating a new UI widget | UI_WIDGETS.md, UI_THEMES.md |
| Setting up navigation | UI_NAVIGATION.md, UI_DESIGN.md |
| Adding animations | UI_ANIMATIONS.md, UI_DESIGN.md |
| Integrating an API | API_INTEGRATIONS.md, COMMUNICATION_PROTOCOL.md |
| Debugging | IMPLEMENTATION_GUIDE.md (Testing section) |

### For Implementation

1. Follow [IMPLEMENTATION_GUIDE.md](./IMPLEMENTATION_GUIDE.md) step-by-step
2. Reference widget code from [UI_WIDGETS.md](./UI_WIDGETS.md)
3. Use navigation patterns from [UI_NAVIGATION.md](./UI_NAVIGATION.md)
4. Apply themes from [UI_THEMES.md](./UI_THEMES.md)
5. Add animations from [UI_ANIMATIONS.md](./UI_ANIMATIONS.md)

---

## Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                        HOST COMPUTER                              │
│  ┌─────────────────────┐      ┌─────────────────────┐            │
│  │   C++ Server         │      │  External APIs       │            │
│  │                     │      │                     │            │
│  │  - HTTP Requests     │◄─────┤  OpenWeatherMap    │            │
│  │  - File Monitoring   │◄─────┤  Thunderbird       │            │
│  │  - System API        │◄─────┤  Spotify           │            │
│  │  - Media Player API  │◄─────┤  Portfolio API     │            │
│  │                     │      │                     │            │
│  │  Encodes Data        │      │                     │            │
│  └──────────┬──────────┘      └─────────────────────┘            │
│              │                                                   │
│              ▼                                                   │
│  ┌─────────────────────┐                                        │
│  │   USB Serial Port    │                                        │
│  └──────────┬──────────┘                                        │
└─────────────┼────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────┴────────────────────────────────────────────────────┐
│                        ESP32-4848S040                              │
│  ┌─────────────────────┐      ┌─────────────────────┐            │
│  │   Serial Handler     │      │  Display + LVGL      │            │
│  │                     │      │                     │            │
│  │  Decodes Messages    │─────►│  - Rendering        │            │
│  │  Updates Data Store  │      │  - Touch Handling   │            │
│  │  Processes Commands  │      │  - Animations        │            │
│  │                     │      │                     │            │
│  └─────────────────────┘      └─────────────────────┘            │
│                                                                     │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                    UI Manager                                │  │
│  │  - Home Screen       │  - Weather Screen     │  - Media Screen │  │
│  │  - Portfolio Screen  │  - Notifications Screen│  - Settings     │  │
│  └────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Message Flow

```
HOST → ESP32 (Server to Device)
  1. HEARTBEAT (keep-alive)
  2. TIME (current time)
  3. WEATHER (weather data)
  4. MEDIA (media info)
  5. NOTIFICATION (new notifications)
  6. PORTFOLIO (portfolio data)
  7. SYSTEM (system stats)
  8. CONFIG (configuration)

ESP32 → HOST (Device to Server)
  1. HEARTBEAT (acknowledgment)
  2. ACK (message received)
  3. NACK (message error)
  4. REQUEST (data request)
  5. TOUCH (touch event)
  6. COMMAND (user command)
  7. DEBUG (debug info)
```

---

## Key Classes and Components

### Server Side

| Component | File | Description |
|-----------|------|-------------|
| Server | `server.hpp/cpp` | Main application class |
| SerialPort | `serial_port.hpp/cpp` | Serial communication handler |
| Protocol | `protocol.hpp/cpp` | Message encoding/decoding |
| DataSource | `data_source.hpp` | Base class for data providers |
| WeatherDataSource | `weather.hpp/cpp` | Weather data from API |
| MediaDataSource | `media.hpp/cpp` | Media player integration |
| NotificationDataSource | `notifications.hpp/cpp` | Email/notification monitoring |
| PortfolioDataSource | `portfolio.hpp/cpp` | Portfolio API integration |
| SystemMonitorDataSource | `system_monitor.hpp/cpp` | System statistics |

### ESP32 Side

| Component | File | Description |
|-----------|------|-------------|
| Display | `display.hpp/cpp` | Display and touch driver |
| SerialManager | `serial.hpp/cpp` | Serial communication handler |
| UIManager | `ui_manager.hpp/cpp` | Manages screens and UI |
| DataStore | `data_store.hpp/cpp` | Central data storage |
| HomeScreen | `home_screen.hpp/cpp` | Home screen implementation |
| WeatherScreen | `weather_screen.hpp/cpp` | Weather detail screen |
| MediaScreen | `media_screen.hpp/cpp` | Media player screen |
| PortfolioScreen | `portfolio_screen.hpp/cpp` | Portfolio detail screen |
| NotificationScreen | `notifications_screen.hpp/cpp` | Notifications list |
| SettingsScreen | `settings_screen.hpp/cpp` | Settings screen |

### Widgets

| Widget | File | Description |
|--------|------|-------------|
| TimeWidget | `time_widget.hpp/cpp` | Time and date display |
| WeatherWidget | `weather_widget.hpp/cpp` | Weather information display |
| MediaWidget | `media_widget.hpp/cpp` | Media player widget |
| PortfolioWidget | `portfolio_widget.hpp/cpp` | Portfolio summary |
| NotificationWidget | `notification_widget.hpp/cpp` | Notification display |
| SystemWidget | `system_widget.hpp/cpp` | System statistics |
| CardWidget | `card_widget.hpp/cpp` | Generic card container |
| BadgeWidget | `badge_widget.hpp/cpp` | Notification badge |
| ProgressBarWidget | `progress_bar_widget.hpp/cpp` | Progress bar |
| ChartWidget | `chart_widget.hpp/cpp` | Data chart |
| GaugeWidget | `gauge_widget.hpp/cpp` | Circular gauge |

---

## Configuration Files

### Server Configuration

**server/config/config.hpp**
```cpp
constexpr const char* DEFAULT_CONFIG_FILE = "config.json";
constexpr uint32_t DEFAULT_BAUD_RATE = 115200;
constexpr const char* DEFAULT_PORT = "COM3"; // or /dev/ttyUSB0
```

**server/config/settings.json**
```json
{
  "server": {
    "port": "COM3",
    "baud_rate": 115200,
    "update_interval": 100
  },
  "integrations": {
    "weather": { "enabled": true, "api_key": "...", "location": "..." },
    "thunderbird": { "enabled": true, "mail_directory": "..." },
    "media_player": { "enabled": true, "preferred_player": "spotify" },
    "portfolio": { "enabled": true, "api_url": "..." },
    "system_monitor": { "enabled": true }
  }
}
```

### ESP32 Configuration

**firmware/src/config/pins.hpp**
```cpp
// Display
#define TFT_CS   5
#define TFT_DC   21
// ... other display pins

// Touch
#define TOUCH_CS  27
#define TOUCH_IRQ 34
```

**firmware/src/config/settings.hpp**
```cpp
#define SERIAL_BAUD_RATE 115200
#define DISPLAY_WIDTH 480
#define DISPLAY_HEIGHT 480
#define UI_UPDATE_INTERVAL 16
```

---

## Quick Start for Developers

### Prerequisites

1. **Hardware**: ESP32-4848S040 board with 480x480 display
2. **Software** (Server) — exact configured setup in [DEV_ENVIRONMENT.md](./DEV_ENVIRONMENT.md):
   - Visual Studio 2022 with "Desktop development with C++" (MSVC + Windows SDK)
   - CMake 3.15+ (3.29.3 installed) and Ninja (optional)
   - Git
   - vcpkg at `C:\vcpkg` with `VCPKG_ROOT` set and triplet `x64-windows`
3. **Software** (ESP32):
   - PlatformIO
   - Python 3.8+ (project scripts use the `le_dashboard_env` conda env only)
   - ESP32 toolchain

### Setup

```bash
# Clone the project
git clone <repository_url>
cd LeDashboard

# Configure + build the server (deps from vcpkg.json auto-install via vcpkg manifest mode)
cd server
cmake --preset windows-ninja   # or: cmake --preset windows-msvc
cmake --build --preset windows-ninja

# Set up ESP32 firmware
cd ../firmware
pio lib install
```

### Build

```bash
# Server (from server/)
cmake --build --preset windows-ninja   # or: cmake --build --preset windows-msvc

# ESP32
cd ../firmware
pio run -t build
```

### Flash ESP32

```bash
cd firmware
pio run -t upload
```

### Run Server

```bash
# Windows
./server/build/Release/dashboard_server.exe

# Linux/macOS
./server/build/dashboard_server
```

---

## Common Commands

### PlatformIO

```bash
# Build
pio run -t build

# Upload
pio run -t upload

# Monitor serial output
pio device monitor

# Clean
pio run -t clean

# List libraries
pio lib list

# Update libraries
pio lib update
```

### CMake

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Install
cmake --install build

# Clean
cmake --build build --target clean
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2024-XX-XX | Initial context documentation created |

---

## Contributing

When contributing to this project:

1. **Follow existing patterns** - Use the same coding style and architecture
2. **Update documentation** - Keep these context files up to date
3. **Test on hardware** - Always verify on actual ESP32-4848S040 board
4. **Optimize for constraints** - Consider memory and performance limitations
5. **Document changes** - Add comments and update context files

---

## License

This documentation is provided as-is for use with the ESP32 Dashboard project. See the main repository for licensing information.

---

## Summary

This context documentation provides everything you need to:

✅ **Understand** the project architecture and requirements
✅ **Reference** technical specifications and constraints
✅ **Implement** features following established patterns
✅ **Integrate** external APIs and services
✅ **Design** UI elements with consistent styling
✅ **Troubleshoot** common issues

**Next Step**: When starting a new session, simply ask Vibe to "read the context files" or reference specific files as needed.
