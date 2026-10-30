# ESP32 Dashboard Project - Overview

## Project Description

This is a **hardware dashboard** system for a computer, built around an **ESP32-4848S040** development board. The ESP32 is directly wired to a **480x480 pixel touchscreen display**, creating a dedicated, low-latency interface for displaying real-time information from the host computer.

The system follows a **client-server architecture** where:
- **Server (C++ Application)**: Runs on the host computer, collects data from various sources
- **Client (ESP32 Firmware)**: Runs on the microcontroller, displays data on the touchscreen via LVGL
- **Communication**: Bidirectional USB serial protocol for data exchange

## Hardware Specification

| Component | Details |
|-----------|---------|
| **Microcontroller** | ESP32-4848S040 (ESP32-based board) |
| **Display** | 480x480 pixel TFT LCD with touch capability |
| **Interface** | USB Serial (UART) |
| **Resolution** | 480x480 pixels |
| **Color Depth** | 16-bit (65,536 colors) or 24-bit (16.7M colors) |
| **Touch Input** | Resistive or Capacitive (board-dependent) |

## Software Stack

### Host Computer (C++ Server)
- **Language**: C++ (for performance and low resource usage)
- **Purpose**: Data aggregation, processing, and serial communication
- **Architecture**: Background daemon/service
- **Dependencies**: 
  - Serial port library (e.g., Boost.Asio, libserial, or platform-specific)
  - JSON library for data serialization (e.g., nlohmann/json)
  - Platform-specific APIs for data collection

### ESP32 Firmware (Client)
- **Development Environment**: PlatformIO
- **Graphics Library**: LVGL (Light and Versatile Graphics Library)
- **Touch Library**: XPT2046 or equivalent (depending on touch controller)
- **Display Driver**: SPI-based TFT driver (e.g., ST7796, ILI9341, or board-specific)
- **Communication**: Hardware Serial (UART) with buffering

### Development Tools
- **PlatformIO**: For ESP32 firmware development and deployment
- **LVGL**: For rich GUI creation with widgets, animations, and themes
- **C++ Compiler**: MSVC (Visual Studio 2022) for host application
- **Build System**: CMake presets (`server/CMakePresets.json`) with Visual Studio 2022 or Ninja
- **Dependencies**: vcpkg manifest mode (`server/vcpkg.json`), installed at `C:\vcpkg` with `VCPKG_ROOT` set
- **Testing**: com0com virtual COM pair (COM20 <-> COM21) + mock ESP32 script (`server/tools/mock_esp32.py`) using the `le_dashboard_env` conda Python
- See [DEV_ENVIRONMENT.md](./DEV_ENVIRONMENT.md) for the full configured environment

## Architecture Diagram

```
+------------------+     +---------------------+     +------------------+
|                  |     |                     |     |                  |
|  Data Sources    +---->+  C++ Server App     +---->+  ESP32 Firmware  |
|  (APIs, Local    |     |  (Data Aggregator)  |     |  (LVGL Display)  |
|   Services)      |     |                     |     |                  |
+------------------+     +----------+----------+     +----------+----------+
                                      |                        |
                                      v                        v
                            +----------+----------+   +----------+----------+
                            |  USB Serial Protocol |   |  LVGL Rendering |
                            |  (Bidirectional)      |   |  (Touch + Display) |
                            +---------------------+   +------------------+
```

## Communication Flow

1. **Data Collection**: C++ server collects data from various sources (weather APIs, media players, email clients, portfolio APIs)
2. **Data Processing**: Server processes and formats data into a standardized protocol
3. **Data Transmission**: Server sends data packets to ESP32 via USB serial
4. **Data Reception**: ESP32 receives and parses incoming data
5. **UI Update**: ESP32 updates LVGL widgets with new data
6. **User Interaction**: Touch events on ESP32 are sent back to server (if needed)

## Design Principles

1. **Performance First**: Minimize latency in data display and user interaction
2. **Resource Efficiency**: Low CPU/memory usage on both host and ESP32
3. **Modularity**: Each feature is independent and can be enabled/disabled
4. **Extensibility**: Easy to add new data sources and UI elements
5. **Reliability**: Robust error handling and recovery mechanisms
6. **Real-time Updates**: Sub-second refresh rates for critical information

## Project Goals

- Create a **responsive, touch-enabled dashboard** displaying real-time information
- Maintain **low resource usage** on the host computer
- Provide **smooth animations and transitions** via LVGL
- Support **multiple data sources** simultaneously
- Enable **customizable layouts** and themes
- Ensure **stable, long-running operation** without crashes

## Next Steps

See the following context files for detailed specifications:
- [FEATURES.md](./FEATURES.md) - Detailed feature descriptions
- [TECHNICAL_SPECIFICATIONS.md](./TECHNICAL_SPECIFICATIONS.md) - Hardware and software details
- [COMMUNICATION_PROTOCOL.md](./COMMUNICATION_PROTOCOL.md) - Serial communication protocol
- [API_INTEGRATIONS.md](./API_INTEGRATIONS.md) - External API specifications
- [UI_DESIGN.md](./UI_DESIGN.md) - LVGL interface design
- [IMPLEMENTATION_GUIDE.md](./IMPLEMENTATION_GUIDE.md) - Implementation strategies
