# LeDashboard Firmware

ESP32 firmware for the LeDashboard project with LVGL-based UI.

## Project Structure

```
firmware/
├── include/
│   ├── data/              # Data structures
│   │   ├── time_data.h
│   │   ├── weather_data.h
│   │   └── ...
│   ├── ui/               # UI classes
│   │   ├── page_base.h
│   │   ├── time_weather_page.h
│   │   └── ...
│   ├── protocol.h        # Communication protocol
│   └── lv_conf.h         # LVGL configuration
├── src/
│   ├── data/             # Data implementations
│   │   ├── dummy_time.cpp
│   │   ├── dummy_weather.cpp
│   │   └── ...
│   ├── ui/              # UI implementations
│   │   ├── page_base.cpp
│   │   ├── time_weather_page.cpp
│   │   └── ...
│   └── main.cpp          # Main application
├── lib/                 # External libraries
│   └── Touch_GT911/      # GT911 touch controller library
├── platformio.ini       # PlatformIO configuration
└── default.csv          # Partition table
```

## Hardware

- **Board**: ESP32-4848S040
- **Display**: 480x480 TFT LCD (parallel interface)
- **Touch**: GT911 capacitive touch controller (I2C)

## Pin Configuration

### Display (Parallel Interface)
- TFT_CS: 5
- TFT_DC: 21
- TFT_WR: 22
- TFT_RD: 19
- TFT_D0: 23
- TFT_D1: 25
- TFT_D2: 26
- TFT_D3: 27
- TFT_D4: 12
- TFT_D5: 13
- TFT_D6: 14
- TFT_D7: 15
- TFT_BL: 4

### Touch (GT911)
- SDA: 22 (configurable)
- SCL: 17 (configurable)
- INT: -1 (not connected)
- RST: -1 (not connected)

## Current Implementation

### Time & Weather Page
The first page displays:
- **Left Column**:
  - Date in French format (e.g., "Jeudi 24 octobre 2026")
  - Time in 24h format (e.g., "13:40")
  - Weather icon
  - Current temperature
  - Humidity (RH)
  - Air Quality (QA)
  - UV Index
- **Right Column**:
  - Tomorrow's weather icon
  - Min/Max temperature for tomorrow

### Dummy Data
Currently using dummy data sources for testing:
- `dummy_time.cpp`: Provides static time data
- `dummy_weather.cpp`: Provides static weather data

### French Localization
- Day names: Lundi, Mardi, Mercredi, Jeudi, Vendredi, Samedi, Dimanche
- Month names: janvier, février, mars, avril, mai, juin, juillet, août, septembre, octobre, novembre, décembre
- Air quality strings in French
- UV index strings in French

## Build Configuration

The `platformio.ini` file is configured for:
- Board: ESP32-S3 (esp32-s3-devkitm-1 variant)
- Display: Parallel 8-bit interface with ST7796 driver
- LVGL: Version 8.3.0 with Montserrat fonts
- Memory: 16MB flash, PSRAM enabled

## TODO

1. **Fix TFT_eSPI Configuration**: Verify parallel interface configuration
2. **Add Touch Support**: Integrate GT911 touch controller
3. **Add Fonts**: Include Montserrat font files for all sizes
4. **Test Display**: Get basic display working without LVGL
5. **Test LVGL**: Get LVGL rendering working
6. **Add Page Navigation**: Implement swipe gestures for page switching
7. **Add Media Player Overlay**: Implement media player module
8. **Add Notifications**: Implement notification popup system
9. **Add Other Pages**: Portfolio, System Monitor, Calendar

## Known Issues

1. Compilation errors related to font declarations
2. TFT_eSPI pin configuration for parallel interface
3. PlatformIO not available in current environment for testing
