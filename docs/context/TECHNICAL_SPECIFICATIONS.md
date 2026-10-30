# Technical Specifications

## Hardware Specifications

### ESP32-4848S040 Board

#### Microcontroller
| Specification | Value |
|---------------|-------|
| **Model** | ESP32 (specific variant depends on board) |
| **CPU** | Xtensa Dual-Core 32-bit LX6 (or single-core) |
| **Clock Speed** | 80 MHz - 240 MHz (configurable) |
| **Flash Memory** | 4 MB - 16 MB (external SPI) |
| **SRAM** | 520 KB |
| **PSRAM** | 0 - 8 MB (optional, board-dependent) |
| **GPIO Pins** | 30+ (varies by board) |
| **UART Ports** | 3 (UART0, UART1, UART2) |
| **SPI Ports** | 4 (SPI, HSPI, VSPI, I2S) |
| **I2C Ports** | 2 |
| **ADC Channels** | 16 (12-bit, 0-3.3V) |
| **DAC Channels** | 2 (8-bit) |
| **Touch Pads** | 10 (capacitive) |

#### Power
| Specification | Value |
|---------------|-------|
| **Input Voltage** | 5V (USB) or 3.3V |
| **Operating Voltage** | 3.3V |
| **Power Consumption** | 80-500 mA (varies by activity) |
| **Deep Sleep Current** | ~5-150 uA |

### Display Module

#### Physical Characteristics
| Specification | Value |
|---------------|-------|
| **Resolution** | 480x480 pixels |
| **Display Type** | TFT LCD (IPS or TN panel) |
| **Color Depth** | 16-bit (65,536 colors) or 24-bit (16.7M colors) |
| **Size** | ~3.5" to 5" diagonal |
| **Pixel Pitch** | ~0.088mm x 0.088mm |
| **Aspect Ratio** | 1:1 (Square) |
| **Viewing Angle** | Depends on panel type (IPS: 178°, TN: ~160°) |

#### Interface
| Specification | Value |
|---------------|-------|
| **Interface Type** | SPI (most common) or Parallel |
| **SPI Speed** | Up to 80 MHz |
| **Data Lines** | MOSI, MISO, SCK, CS, DC, RESET |
| **Display Driver** | Varies by board (ST7796, ILI9341, GC9A01, etc.) |

#### Touch Panel
| Specification | Value |
|---------------|-------|
| **Type** | Resistive or Capacitive |
| **Controller** | XPT2046 (resistive) or FT6236/GT911 (capacitive) |
| **Interface** | SPI or I2C |
| **Resolution** | 4096x4096 (typical for XPT2046) |
| **Sampling Rate** | ~125-250 samples/second |
| **Accuracy** | ±1-2mm (resistive), ±1mm (capacitive) |

### Memory Requirements

#### ESP32 Firmware
| Component | Size Estimate |
|-----------|---------------|
| **Bootloader** | 16-32 KB |
| **Partition Table** | 4 KB |
| **Firmware (Code)** | 200-800 KB |
| **LVGL Library** | 100-300 KB |
| **Display Driver** | 10-50 KB |
| **Touch Driver** | 10-30 KB |
| **Fonts** | 20-200 KB |
| **Images** | 50-500 KB |
| **SPIFFS (Storage)** | 500 KB - 2 MB |
| **Heap (Runtime)** | 50-200 KB |
| **Stack** | 4-16 KB per task |

#### Recommended Partition Table
```
Partition Table:
- Bootloader: 0x1000 (4 KB)
- Partition Table: 0x8000 (32 KB)
- Storage (SPIFFS): 0x10000-0x20000 (2 MB)
- OTA Data: 0x21000-0x31000 (1 MB)
- OTA App: 0x31000-0x41000 (1 MB)
- App: 0x41000-0x51000 (1 MB)
```

### Performance Targets

#### Rendering Performance
| Metric | Target | Notes |
|--------|--------|-------|
| **Frame Rate** | 30-60 FPS | For smooth animations |
| **Widget Updates** | <16ms per widget | For 60 FPS |
| **Screen Redraw** | <50ms | Full screen refresh |
| **Touch Latency** | <50ms | From touch to visual feedback |
| **Animation Smoothness** | 60 FPS | For all animations |

#### Memory Usage
| Metric | Target | Notes |
|--------|--------|-------|
| **Heap Free** | >100 KB | At all times |
| **Stack Usage** | <80% | Per task |
| **Fragmentation** | <5% | Heap fragmentation |

#### Data Processing
| Metric | Target | Notes |
|--------|--------|-------|
| **Serial Parse Time** | <10ms | Per message |
| **JSON Parse Time** | <20ms | For 2KB message |
| **Data Update Latency** | <50ms | From receipt to display |

---

## Software Specifications

### Host Application (C++ Server)

#### Platform Support
| Platform | Support Level | Notes |
|----------|---------------|-------|
| **Windows 10/11** | Full | Primary target |
| **Windows 8/8.1** | Limited | May require adjustments |
| **Linux** | Experimental | Serial port differences |
| **macOS** | Experimental | Serial port differences |

#### Dependencies

##### Required
| Library | Version | Purpose |
|---------|---------|---------|
| **C++ Standard Library** | C++17 | Core functionality |
| **Serial Port Library** | Latest | USB communication |
| **JSON Library** | Latest | Data serialization |

##### Recommended
| Library | Version | Purpose |
|---------|---------|---------|
| **Boost.Asio** | 1.70+ | Cross-platform serial |
| **nlohmann/json** | 3.10+ | JSON parsing |
| **spdlog** | 1.9+ | Logging |
| **fmt** | 8.0+ | String formatting |
| **cpr** | Latest | HTTP requests |
| **libcurl** | Latest | Alternative HTTP |

##### Optional
| Library | Version | Purpose |
|---------|---------|---------|
| **SQLite** | 3.x | Local data caching |
| **OpenSSL** | 1.1+ | HTTPS support |
| **Windows API** | Native | Windows-specific features |

#### Build Configuration
```cmake
# Example CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(DashboardServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Add executable
add_executable(dashboard_server main.cpp)

# Link libraries
target_link_libraries(dashboard_server PRIVATE Threads::Threads)

# Platform-specific settings
if(WIN32)
    target_link_libraries(dashboard_server PRIVATE Setupapi)
endif()
```

#### Application Structure
```
dashboard_server/
├── CMakeLists.txt
├── main.cpp                 # Entry point
├── config/
│   ├── config.hpp          # Configuration constants
│   └── settings.hpp        # User settings
├── core/
│   ├── server.hpp          # Main server class
│   └── serial_port.hpp     # Serial communication
├── data/
│   ├── weather.hpp         # Weather data collector
│   ├── media.hpp           # Media player integration
│   ├── notifications.hpp   # Notification handler
│   └── portfolio.hpp       # Portfolio data
├── protocol/
│   ├── protocol.hpp        # Communication protocol
│   ├── encoder.hpp         # Message encoding
│   └── decoder.hpp         # Message decoding
├── utils/
│   ├── logger.hpp          # Logging utility
│   ├── json_helper.hpp     # JSON utilities
│   └── time_utils.hpp      # Time utilities
└── tests/
    └── ...                 # Unit tests
```

### ESP32 Firmware (PlatformIO)

#### PlatformIO Configuration
```ini
; platformio.ini
[env:esp32-4848s040]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

; Custom board configuration
board_build.partitions = custom_partitions.csv
board_build.f_cpu = 240000000L

; Memory settings
board_build.ldscript = custom.ld

; Libraries
lib_deps =
    lovyan03/LovyanGFX @ ^1.1.5
    lvgl/lvgl @ ^8.3.0
   lvgl/lvgl_esp32_drivers @ ^1.0.0
    bodmer/TFT_eSPI @ ^2.4.76
    adafruit/Adafruit BusIO @ ^1.12.0
    bblanchon/ArduinoJson @ ^6.20.0

; Build flags
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DLVGL_USE_DISP_DRAW_BUF
    -DLVGL_DRAW_BUF_SIZE=8192
    -DLVGL_DRAW_BUF_NEXT_LINE
    -DLV_CFLP_SKIP

; Upload settings
upload_protocol = esptool
upload_port = COM3
upload_speed = 921600

; Debug settings
debug_tool = openocd
```

#### Custom Partitions
```csv
# custom_partitions.csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
phy_init, data, phy,     0xe000,  0x1000,
factory,  app,  factory,  0x10000, 1M,
storage,  data, 0x99,    0x110000, 2M,
```

#### Firmware Structure
```
lib/
├── lvgl/                     # LVGL library
│   ├── lvgl/                 # Core LVGL
│   └── lvgl_esp32_drivers/   # ESP32 drivers
├── TFT_eSPI/                # Display library
├── ArduinoJson/             # JSON parsing
└── ...

src/
├── main.cpp                 # Entry point
├── config/
│   ├── pins.hpp             # Pin definitions
│   ├── settings.hpp         # Configuration
│   └── display_config.hpp   # Display settings
├── display/
│   ├── display.hpp          # Display driver
│   ├── touch.hpp            # Touch driver
│   └── lvgl_helper.hpp      # LVGL utilities
├── ui/
│   ├── theme.hpp            # LVGL theme
│   ├── screens/             # Screen implementations
│   │   ├── home_screen.hpp  # Home screen
│   │   ├── weather_screen.hpp
│   │   ├── media_screen.hpp
│   │   └── ...
│   ├── widgets/             # Custom widgets
│   │   ├── weather_widget.hpp
│   │   ├── media_widget.hpp
│   │   └── ...
│   └── ui_manager.hpp       # UI state management
├── serial/
│   ├── serial.hpp           # Serial communication
│   ├── protocol.hpp         # Protocol handler
│   └── message_handler.hpp  # Message processing
├── data/
│   ├── data_store.hpp       # Data storage
│   ├── weather_data.hpp     # Weather data model
│   ├── media_data.hpp       # Media data model
│   └── ...
├── utils/
│   ├── logger.hpp           # Logging
│   ├── timer.hpp            # Timer utilities
│   └── ...
└── tasks/
    ├── display_task.hpp     # Display update task
    ├── serial_task.hpp      # Serial communication task
    └── touch_task.hpp        # Touch input task
```

#### LVGL Configuration

**lv_conf.h** (Key Configuration)
```c
#ifndef LV_CONF_H
#define LV_CONF_H

// Display settings
#define LV_HOR_RES_MAX          (480)
#define LV_VER_RES_MAX          (480)
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP         1

// Memory settings
#define LV_MEM_SIZE             (64 * 1024)  // 64KB for LVGL objects
#define LV_MAX_DISP_BUF_SIZE    (8 * 1024)  // 8KB display buffer

// Performance settings
#define LV_DISP_DEF_REFR_PERIOD 16      // ~60 FPS
#define LV_ANIM_ENABLE           1

// Feature enables
#define LV_USE_THEMES            1
#define LV_THEME_DEFAULT_INIT    lv_theme_default_init
#define LV_USE_GROUP             1
#define LV_USE_OBJ_REALIGN       1

// Widget enables
#define LV_USE_LABEL            1
#define LV_USE_BTN              1
#define LV_USE_IMG              1
#define LV_USE_BAR              1
#define LV_USE_LINE             1
#define LV_USE_CHART            1
#define LV_USE_TABLE            1
#define LV_USE_LIST             1
#define LV_USE_DROPDOWN         1
#define LV_USE_ROLLER           1
#define LV_USE_SLIDER           1
#define LV_USE_SWITCH           1
#define LV_USE_CHECKBOX         1
#define LV_USE_TEXTAREA         1
#define LV_USE_KEYBOARD         1
#define LV_USE_CALENDAR         1
#define LV_USE_GAUGE            1
#define LV_USE_ARC              1
#define LV_USE_PRELOADER        1
#define LV_USE_TABVIEW          1
#define LV_USE_TILEVIEW         1
#define LV_USE_WIN              1
#define LV_USE_SPINBOX          1

#endif // LV_CONF_H
```

#### Display Driver Configuration

**TFT_eSPI Configuration (User_Setup.h)**
```c
// ESP32-4848S040 specific configuration
#define USER_SETUP_ID 23

#define ESP32_PARALLEL
#define TFT_CS   5
#define TFT_DC   21
#define TFT_WR   22
#define TFT_RD   19
#define TFT_D0   23
#define TFT_D1   25
#define TFT_D2   26
#define TFT_D3   27
#define TFT_D4   12
#define TFT_D5   13
#define TFT_D6   14
#define TFT_D7   15

// OR for SPI interface:
// #define TFT_SPI_OVERLAP
// #define TFT_MOSI 23
// #define TFT_MISO 19
// #define TFT_SCLK 18
// #define TFT_CS   5
// #define TFT_DC   21
// #define TFT_RST  22

#define LOAD_GLCD  // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2 // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4 // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6 // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7 // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT8 // Font 8. Large 75 pixel font needs ~3257 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_GFXFF // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY 2500000

// Touch screen configuration
#define TOUCH_CS 27
#define TOUCH_IRQ 34
#define TOUCH_CALIBRATE
```

---

## Communication Protocol

### Serial Port Configuration

#### Physical Connection
| Connection | ESP32 Pin | Computer | Notes |
|------------|-----------|----------|-------|
| **TX** | GPIO1 (UART0_TX) | RX | ESP32 -> Computer |
| **RX** | GPIO3 (UART0_RX) | TX | Computer -> ESP32 |
| **GND** | GND | GND | Common ground |
| **5V** | 5V (optional) | 5V | Power (if needed) |

#### Serial Settings
| Setting | Value | Notes |
|---------|-------|-------|
| **Baud Rate** | 115200 | Default, configurable |
| **Data Bits** | 8 | Standard |
| **Stop Bits** | 1 | Standard |
| **Parity** | None | Standard |
| **Flow Control** | None | No hardware flow control |
| **Buffer Size** | 1024-4096 | Circular buffer |

#### Alternative Baud Rates
- **9600**: Slow, for debugging
- **19200**: Medium speed
- **38400**: Faster
- **57600**: Good balance
- **115200**: Default, recommended
- **230400**: Fast, may need optimization
- **460800**: Very fast, may need ESP32 optimization
- **921600**: Maximum reliable for ESP32

### Message Format

#### Message Structure
```
+--------+--------+--------+--------+--------+--------+
| SOF    | Type   | Length | Payload| CRC    | EOF    |
| 1 byte | 1 byte | 2 bytes| N bytes| 2 bytes| 1 byte |
+--------+--------+--------+--------+--------+--------+

SOF (Start of Frame): 0xAA
Type: Message type identifier
Length: Payload length (big-endian)
Payload: Message data
CRC: CRC-16 checksum
EOF (End of Frame): 0x55
```

#### Message Types
| Type | Name | Description | Direction |
|------|------|-------------|-----------|
| 0x00 | HEARTBEAT | Keep-alive ping | Both |
| 0x01 | TIME | Time and date update | Host -> ESP32 |
| 0x02 | WEATHER | Weather data | Host -> ESP32 |
| 0x03 | MEDIA | Media player info | Host -> ESP32 |
| 0x04 | NOTIFICATION | Notification data | Host -> ESP32 |
| 0x05 | PORTFOLIO | Portfolio data | Host -> ESP32 |
| 0x06 | SYSTEM | System monitor data | Host -> ESP32 |
| 0x07 | CALENDAR | Calendar data | Host -> ESP32 |
| 0x08 | ACK | Acknowledgment | Both |
| 0x09 | NACK | Negative acknowledgment | Both |
| 0x0A | ERROR | Error message | Both |
| 0x0B | CONFIG | Configuration | Both |
| 0x0C | REQUEST | Data request | ESP32 -> Host |
| 0x0D | TOUCH | Touch event | ESP32 -> Host |
| 0x0E | COMMAND | Command execution | Both |
| 0x0F | DEBUG | Debug message | Both |

#### Payload Format

##### JSON-Based (Recommended)
```json
{
  "type": "weather",
  "timestamp": 1633046400,
  "data": {
    "current": {
      "temperature": 22.5,
      "humidity": 65,
      "condition": "Clear",
      "icon": "sun"
    },
    "forecast": [...]
  }
}
```

##### Binary Format (Efficient)
```c
// Example: Time message (Type 0x01)
struct TimeMessage {
    uint32_t timestamp;    // Unix timestamp
    uint8_t hour;         // 0-23
    uint8_t minute;       // 0-59
    uint8_t second;       // 0-59
    uint8_t day;          // 1-31
    uint8_t month;        // 1-12
    uint16_t year;        // 2000-2099
    uint8_t day_of_week;  // 0-6 (Sunday=0)
};
```

### Communication Flow

#### Initial Handshake
```
ESP32: -> [0x00] HEARTBEAT (ping)
Host:  -> [0x00] HEARTBEAT (pong) + [0x0B] CONFIG (capabilities)
ESP32: -> [0x08] ACK
Host:  -> [0x01] TIME (initial time sync)
ESP32: -> [0x08] ACK
```

#### Normal Operation
```
Host:  -> [0x01] TIME (every second)
Host:  -> [0x02] WEATHER (every 10 minutes)
Host:  -> [0x03] MEDIA (every second when playing)
Host:  -> [0x04] NOTIFICATION (when new)
Host:  -> [0x05] PORTFOLIO (every 30 seconds)
ESP32: -> [0x08] ACK (for each message)
ESP32: -> [0x0D] TOUCH (when user interacts)
```

#### Error Handling
```
Host:  -> [0x02] WEATHER (corrupted)
ESP32: -> [0x09] NACK (error code)
Host:  -> [0x02] WEATHER (retry)
ESP32: -> [0x08] ACK
```

### Buffer Management

#### ESP32 Side
```c
#define SERIAL_BUFFER_SIZE 4096
uint8_t serial_buffer[SERIAL_BUFFER_SIZE];
uint16_t buffer_head = 0;
uint16_t buffer_tail = 0;

// Receive interrupt handler
void IRAM_ATTR serial_isr() {
    while (Serial.available()) {
        uint16_t next = (buffer_head + 1) % SERIAL_BUFFER_SIZE;
        if (next != buffer_tail) {
            serial_buffer[buffer_head] = Serial.read();
            buffer_head = next;
        }
    }
}
```

#### Host Side
```cpp
const size_t BUFFER_SIZE = 8192;
std::vector<uint8_t> receive_buffer;
receive_buffer.reserve(BUFFER_SIZE);

// Read thread
void read_serial_port() {
    char buffer[256];
    while (running) {
        size_t bytes_read = serial_port.read(buffer, sizeof(buffer));
        if (bytes_read > 0) {
            receive_buffer.insert(receive_buffer.end(), buffer, buffer + bytes_read);
        }
    }
}
```

---

## Memory Optimization

### ESP32 Memory Layout
```
0x3FFE0000 - 0x3FFFFFFF: Internal RAM (520 KB)
├── 0x3FFE0000 - 0x3FFE4000: RTC Slow Memory (8 KB)
├── 0x3FFE4000 - 0x3FFE8000: RTC Fast Memory (16 KB)
└── 0x3FFE8000 - 0x3FFFFFFF: Main RAM (496 KB)
    ├── Heap
    ├── Stack (per task)
    └── Global variables

0x3F400000 - 0x3F7FFFFF: External PSRAM (4 MB, if available)
0x40080000 - 0x403FFFFF: Flash (4 MB - 8 MB)
```

### Memory Allocation Strategies

#### Heap Management
1. **Static Allocation**: Pre-allocate buffers at startup
2. **Pool Allocation**: Use memory pools for frequent allocations
3. **Avoid Fragmentation**: Allocate large blocks, sub-divide manually
4. **Reuse Buffers**: Reuse display and serial buffers

#### LVGL Memory
```c
// Pre-allocate LVGL memory
lv_mem_init();
void *lv_mem = malloc(LV_MEM_SIZE);
lv_mem_set_base(lv_mem);

// Use display buffer efficiently
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];
lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);
```

#### String Handling
- **Avoid std::string**: Use C strings or pre-allocated buffers
- **Static Strings**: Store constant strings in PROGMEM
- **String Pools**: Reuse common strings
- **Fixed Buffers**: Use char arrays with fixed sizes

### Data Compression

#### For Serial Transmission
1. **Delta Encoding**: Send only changed values
2. **Binary Format**: More compact than JSON
3. **Bit Packing**: Use bits for boolean flags
4. **Run-Length Encoding**: For repetitive data
5. **Dictionary Encoding**: For frequent string values

#### Example: Media Progress
```c
// Instead of sending full time every second:
// JSON: {"position": 123, "duration": 245}
// Binary: [position: uint16, duration: uint16] = 4 bytes

// Send delta:
// [delta: int8] = 1 byte (if delta < 128)
```

---

## Power Management

### ESP32 Power States
| State | Current | Wakeup Sources | Notes |
|-------|---------|----------------|-------|
| **Active** | 80-500 mA | Any | Normal operation |
| **Modem Sleep** | ~20 mA | WiFi/BT events | WiFi still active |
| **Light Sleep** | ~0.8 mA | Timer, GPIO, UART | CPU paused |
| **Deep Sleep** | ~5-150 uA | External GPIO, RTC | Full power down |
| **Hibernate** | ~5 uA | RTC timer only | Extreme low power |

### Power Optimization Techniques

#### Display Power
- **Backlight Control**: PWM control of backlight
- **Display Sleep**: Turn off display when idle
- **Auto-Dim**: Reduce brightness after inactivity

#### CPU Power
- **Clock Speed**: Reduce from 240 MHz to 80 MHz
- **Flash Speed**: Reduce SPI flash speed
- **CPU Voltage**: Reduce from 1.1V to 0.8V (if supported)

#### Peripheral Power
- **SPI Speed**: Reduce display SPI speed
- **Touch Sampling**: Reduce touch sampling rate
- **Disable Unused**: Turn off unused peripherals

#### Sleep Modes
```c
// Light sleep example
esp_sleep_enable_timer_wakeup(10 * 1000000); // 10 seconds
gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);
esp_light_sleep_start();

// Deep sleep example (requires external wakeup)
esp_sleep_enable_timer_wakeup(60 * 1000000); // 60 seconds
esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
esp_deep_sleep_start();
```

### Power Profiles
| Profile | CPU Speed | Display | Touch | Update Rate | Current |
|---------|-----------|---------|-------|-------------|---------|
| **Performance** | 240 MHz | Full | Full | 60 FPS | 300-500 mA |
| **Balanced** | 160 MHz | Full | Full | 30 FPS | 150-250 mA |
| **Battery** | 80 MHz | Dimmed | Reduced | 15 FPS | 50-100 mA |
| **Idle** | 80 MHz | Off | Off | 1 FPS | 20-50 mA |
| **Sleep** | Off | Off | Off | N/A | 0.8-20 mA |

---

## Performance Optimization

### Rendering Optimization

#### LVGL-Specific
```c
// Enable double buffering for smoother rendering
lv_disp_set_draw_buf(disp, &draw_buf);

// Use GPU acceleration if available
lv_disp_set_gpu(disp, true);

// Optimize widget refresh
lv_obj_set_flag(widget, LV_OBJ_FLAG_NON_OPA);
```

#### Display Driver
```c
// Use DMA for SPI transfers (if supported)
SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
SPI.transfer(dma_buffer, size);
SPI.endTransaction();

// Optimize display update regions
void updateDisplay(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    tft.startWrite();
    tft.setAddrWindow(x1, y1, x2, y2);
    tft.writePixels(pixels, (x2 - x1 + 1) * (y2 - y1 + 1));
    tft.endWrite();
}
```

### Data Processing Optimization

#### Serial Parsing
```c
// Use direct buffer parsing instead of JSON
void parseWeatherMessage(const uint8_t* data, size_t len) {
    if (len < sizeof(WeatherMessage)) return;
    
    WeatherMessage* msg = (WeatherMessage*)data;
    currentTemp = msg->temperature / 10.0f; // Fixed-point encoding
    humidity = msg->humidity;
    // ...
}
```

#### Touch Processing
```c
// Use interrupt-based touch reading
void IRAM_ATTR touch_isr() {
    if (touchAvailable()) {
        TS_Point p = getTouchPoint();
        if (p.z > TOUCH_THRESHOLD) {
            touchQueue.push(p);
        }
    }
}
```

### Task Scheduling

#### FreeRTOS Configuration
```c
// Custom FreeRTOS config
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 25
#define configMINIMAL_STACK_SIZE 2048

// Task priorities
#define TASK_DISPLAY_PRIORITY 3
#define TASK_SERIAL_PRIORITY 2
#define TASK_TOUCH_PRIORITY 1
```

#### Task Implementation
```c
// Display update task
void displayTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        lv_timer_handler();
        tft.flush();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(16));
    }
}

// Serial read task
void serialTask(void *pvParameters) {
    while (1) {
        processSerialData();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Touch read task
void touchTask(void *pvParameters) {
    while (1) {
        processTouchEvents();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

---

## Debugging & Monitoring

### Debug Output

#### ESP32 Debug
```c
// Debug configuration
#define DEBUG_LEVEL 3 // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose

// Debug macros
#if DEBUG_LEVEL >= 1
#define LOG_ERROR(tag, ...) esp_log_write(ESP_LOG_ERROR, tag, __VA_ARGS__)
#else
#define LOG_ERROR(tag, ...)
#endif

#if DEBUG_LEVEL >= 3
#define LOG_INFO(tag, ...) esp_log_write(ESP_LOG_INFO, tag, __VA_ARGS__)
#else
#define LOG_INFO(tag, ...)
#endif

// Usage
LOG_INFO("MAIN", "Starting display initialization");
LOG_ERROR("SERIAL", "CRC mismatch: expected %04X, got %04X", expected, actual);
```

#### Host Debug
```cpp
// Logging configuration
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

auto console = spdlog::stdout_color_mt("console");
console->set_level(spdlog::level::debug);
console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

// Usage
console->info("Starting server...");
console->error("Failed to open serial port: {}", error);
```

### Performance Monitoring

#### ESP32 Monitoring
```c
// Memory monitoring
task_status_t status;
uwTaskGetInfo(xTaskGetIdleTaskHandle(), &status, pdTRUE, eInvalid);
LOG_INFO("MEM", "Free heap: %d, Min heap: %d", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

// Task monitoring
UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
TaskStatus_t *pxTaskStatusArray = pvPortMalloc(sizeof(TaskStatus_t) * uxArraySize);
uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);
```

#### Host Monitoring
```cpp
// Performance counters
std::atomic<uint64_t> messages_sent{0};
std::atomic<uint64_t> messages_received{0};
std::atomic<uint64_t> bytes_sent{0};
std::atomic<uint64_t> bytes_received{0};

// Timing
auto start = std::chrono::high_resolution_clock::now();
// ... operation ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
```

---

## Security Considerations

### Data Security
1. **No Sensitive Data**: Dashboard should not display sensitive personal data
2. **API Keys**: Store API keys securely, never in plain text in code
3. **Credentials**: Use secure storage for credentials
4. **Network**: Use HTTPS for all API calls

### ESP32 Security
```c
// Secure storage for sensitive data
#include "esp_secure_boot.h"
#include "esp_flash_partitions.h"

// Use NVS (Non-Volatile Storage) for settings
nvs_handle_t nvs_handle;
ESP_ERROR_CHECK(nvs_open("settings", NVS_READWRITE, &nvs_handle));
ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "api_key", api_key));
ESP_ERROR_CHECK(nvs_commit(nvs_handle));
nvs_close(nvs_handle);
```

### Host Security
```cpp
// Secure credential storage
#include <windows.h>
#include <wincred.h>

// Store credential in Windows Credential Manager
void storeCredential(const std::string& name, const std::string& value) {
    CREDENTIAL cred = {0};
    cred.Type = CREDENTIAL_TYPE_GENERIC;
    cred.TargetName = (LPWSTR)name.c_str();
    cred.CredentialBlob = (LPBYTE)value.c_str();
    cred.CredentialBlobSize = value.size();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    CredWrite(&cred, 0);
}
```

---

## Build & Deployment

### Development Workflow

#### Host Application
```bash
# Windows (from server/; deps from vcpkg.json auto-install via the vcpkg toolchain in the presets)
cmake --preset windows-msvc            # Visual Studio 2022 generator, build dir: build/
cmake --build --preset windows-msvc    # builds Release

# or with Ninja (faster)
cmake --preset windows-ninja           # build dir: build-ninja/
cmake --build --preset windows-ninja

# Manual equivalent without presets
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release

# Linux
cmake -B build
cmake --build build

# Cross-compilation (Windows from Linux)
mxw32-g++ -std=c++17 -o dashboard_server.exe main.cpp -lstdc++ -lsetupapi
```

#### ESP32 Firmware
```bash
# PlatformIO
pio run -t build
pio run -t upload
pio run -t monitor

# Manual
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Deployment

#### Host Application
- **Windows**: EXE file + configuration file
- **Linux**: Binary + systemd service
- **Auto-start**: Windows startup folder or systemd

#### ESP32 Firmware
- **OTA Update**: Over-the-air update via WiFi
- **Serial Update**: Via USB serial
- **Backup**: Save current firmware before update

---

## Testing Strategy

### Unit Testing

#### Host Application
```cpp
// Example test using Catch2
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE("Weather data parsing", "[weather]") {
    auto data = parseWeatherJson(R"({"temp":22.5,"humidity":65})");
    REQUIRE(data.temperature == 22.5f);
    REQUIRE(data.humidity == 65);
}
```

#### ESP32 Firmware
```c
// Unity framework for ESP32
#include <unity.h>

void testWeatherMessageParsing() {
    uint8_t data[] = {0xAA, 0x02, 0x00, 0x08, 0x01, 0x67, ...};
    WeatherData result;
    TEST_ASSERT_TRUE(parseWeatherMessage(data, sizeof(data), &result));
    TEST_ASSERT_EQUAL_FLOAT(22.5, result.temperature);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(testWeatherMessageParsing);
    UNITY_END();
}
```

### Integration Testing
1. **Serial Communication**: Verify data exchange
2. **Display Rendering**: Verify LVGL widgets display correctly
3. **Touch Input**: Verify touch events are detected
4. **Data Flow**: End-to-end data from host to display
5. **Error Recovery**: Verify recovery from errors

### Performance Testing
1. **Frame Rate**: Measure actual FPS
2. **Latency**: Measure touch to display latency
3. **Memory Usage**: Monitor heap usage over time
4. **CPU Usage**: Monitor CPU usage on both devices
5. **Battery Life**: Measure power consumption

---

## Versioning

### Semantic Versioning
```
MAJOR.MINOR.PATCH
- MAJOR: Breaking changes, protocol changes
- MINOR: New features, backward compatible
- PATCH: Bug fixes, small improvements
```

### Version History
| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | TBD | Initial implementation |
| 0.2.0 | TBD | Add weather and media |
| 0.3.0 | TBD | Add notifications |
| 0.4.0 | TBD | Add portfolio |
| 1.0.0 | TBD | Stable release |

### Protocol Versioning
```c
#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 0

// Handshake includes protocol version
struct HandshakeMessage {
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint8_t capabilities;
};
```

---

## Maintenance & Support

### Logging Levels
| Level | Description | Use Case |
|-------|-------------|----------|
| **Error** | Critical errors | Cannot recover, needs user action |
| **Warn** | Warning conditions | Non-critical issues, automatic recovery |
| **Info** | Informational | Normal operation messages |
| **Debug** | Debug information | Development debugging |
| **Verbose** | Detailed traces | Deep debugging |

### Error Codes
| Code | Name | Description |
|------|------|-------------|
| 0x00 | SUCCESS | Operation successful |
| 0x01 | INVALID_DATA | Invalid message format |
| 0x02 | CRC_ERROR | CRC checksum mismatch |
| 0x03 | TIMEOUT | Operation timed out |
| 0x04 | BUFFER_OVERFLOW | Buffer overflow |
| 0x05 | NOT_IMPLEMENTED | Feature not implemented |
| 0x06 | INVALID_STATE | Invalid state for operation |
| 0x07 | RESOURCE_ERROR | Resource unavailable |
| 0x08 | COMMUNICATION_ERROR | Serial communication error |
| 0x09 | PARSING_ERROR | Data parsing error |
| 0x0A | MEMORY_ERROR | Memory allocation error |

### Troubleshooting Guide

#### Common Issues

**Display Not Working**
- Check wiring (MOSI, MISO, SCK, CS, DC, RESET)
- Verify display driver configuration
- Check SPI speed settings
- Test with display library examples

**Touch Not Working**
- Check touch controller wiring
- Verify touch calibration
- Check interrupt pin configuration
- Test with touch library examples

**Serial Communication Issues**
- Verify baud rate matches on both ends
- Check TX/RX wiring (cross-connect: ESP32 TX -> Host RX, ESP32 RX -> Host TX)
- Verify common ground connection
- Check for buffer overflows
- Test with simple echo program

**Memory Issues**
- Check heap usage with `esp_get_free_heap_size()`
- Look for memory leaks in custom code
- Reduce LVGL memory usage
- Increase partition sizes

**Performance Issues**
- Reduce update frequency
- Simplify LVGL widgets
- Optimize display driver settings
- Reduce SPI speed
- Check for blocking operations

---

## Hardware Compatibility

### Supported ESP32 Boards
| Board | Display | Touch | Notes |
|-------|---------|-------|-------|
| ESP32-4848S040 | 480x480 | Yes | Primary target |
| ESP32-S3-4848S040 | 480x480 | Yes | ESP32-S3 variant |
| ESP32-2432S028 | 240x240 | Yes | Lower resolution |
| ESP32-3248S035 | 320x480 | Yes | Portrait |
| Custom ESP32 | Custom | Custom | May need configuration |

### Supported Display Controllers
| Controller | Interface | Resolution | Notes |
|------------|-----------|------------|-------|
| ST7796 | SPI | 480x480 | Common |
| ILI9341 | SPI | 320x240 | Common |
| GC9A01 | SPI | 240x240 | Common |
| SSD1963 | Parallel/SPI | 480x480 | High-end |
| RM68140 | SPI | 480x480 | High-end |

### Supported Touch Controllers
| Controller | Interface | Type | Notes |
|------------|-----------|------|-------|
| XPT2046 | SPI | Resistive | Most common |
| FT6236 | I2C | Capacitive | Common |
| GT911 | I2C | Capacitive | Common |
| CST816S | I2C | Capacitive | Common |
| Goodix | I2C | Capacitive | Various models |
