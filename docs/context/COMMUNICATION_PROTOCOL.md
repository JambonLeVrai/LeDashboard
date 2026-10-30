# Communication Protocol Specification

## Overview

This document specifies the **bidirectional serial communication protocol** between the **Host C++ Application** and the **ESP32 Firmware**. The protocol is designed to be:

- **Efficient**: Minimal overhead for high-frequency updates
- **Reliable**: Error detection and recovery mechanisms
- **Extensible**: Easy to add new message types
- **Backward Compatible**: New versions maintain compatibility with older versions
- **Platform Agnostic**: Works across different operating systems

---

## Physical Layer

### Serial Connection

#### Wiring
```
Host Computer          ESP32-4848S040
+------------------+    +------------------+
| TX (Transmit)   |---| RX (GPIO3)       |
| RX (Receive)   |---| TX (GPIO1)       |
| GND            |---| GND              |
| (Optional 5V)  |---| 5V (if needed)   |
+------------------+    +------------------+
```

**Note**: 
- ESP32 UART0 is used by default (GPIO1 = TX, GPIO3 = RX)
- These pins are connected to the USB-to-Serial chip on most ESP32 boards
- For custom boards, check the schematic for UART0 pin assignments

#### Serial Settings
| Parameter | Value | Notes |
|-----------|-------|-------|
| **Baud Rate** | 115200 | Default, configurable (115200, 230400, 460800, 921600) |
| **Data Bits** | 8 | Standard |
| **Stop Bits** | 1 | Standard |
| **Parity** | None | Standard |
| **Flow Control** | None | No hardware flow control |
| **Buffer Size** | 4096 bytes | Minimum recommended |

#### Baud Rate Selection
| Baud Rate | Max Throughput | Notes |
|-----------|----------------|-------|
| 9600 | ~1 KB/s | Debug only |
| 19200 | ~2 KB/s | Slow |
| 38400 | ~4 KB/s | Medium |
| 57600 | ~6 KB/s | Good |
| **115200** | **~12 KB/s** | **Default, recommended** |
| 230400 | ~24 KB/s | Fast, may need optimization |
| 460800 | ~48 KB/s | Very fast, test stability |
| 921600 | ~96 KB/s | Maximum, may be unstable |

**Recommendation**: Start with 115200, increase if bandwidth is insufficient.

---

## Data Link Layer

### Message Framing

#### Frame Structure
```
+--------+--------+--------+--------+--------+--------+
| SOF    | Type   | Length | Payload| CRC    | EOF    |
| 1 byte | 1 byte | 2 bytes| N bytes| 2 bytes| 1 byte |
+--------+--------+--------+--------+--------+--------+
```

| Field | Size | Description |
|-------|------|-------------|
| **SOF** | 1 byte | Start of Frame marker (0xAA) |
| **Type** | 1 byte | Message type identifier |
| **Length** | 2 bytes | Payload length in bytes (big-endian) |
| **Payload** | N bytes | Message payload (0-4095 bytes) |
| **CRC** | 2 bytes | CRC-16 checksum of Type + Length + Payload |
| **EOF** | 1 byte | End of Frame marker (0x55) |

#### Frame Size Limits
- **Minimum Frame**: 6 bytes (SOF + Type + Length(0) + CRC + EOF)
- **Maximum Payload**: 4095 bytes (due to 2-byte length field)
- **Maximum Frame**: 4095 + 6 = 4101 bytes

### Special Bytes
| Byte | Value | Purpose |
|------|-------|---------|
| SOF | 0xAA | Start of frame |
| EOF | 0x55 | End of frame |
| ESC | 0x5A | Escape character for byte stuffing |

### Byte Stuffing

To handle cases where SOF, EOF, or ESC bytes appear in the payload:

1. **Sender**: Replace 0xAA, 0x55, or 0x5A with 0x5A followed by the byte XOR 0xA5
2. **Receiver**: When 0x5A is received, the next byte is XOR 0xA5 to get the original

**Example**:
- Original: 0xAA → Encoded: 0x5A 0x0F (0xAA XOR 0xA5 = 0x0F)
- Original: 0x55 → Encoded: 0x5A 0xFA (0x55 XOR 0xA5 = 0xFA)
- Original: 0x5A → Encoded: 0x5A 0xFF (0x5A XOR 0xA5 = 0xFF)

### CRC Calculation

**Algorithm**: CRC-16/CCITT (0x1021 polynomial, initial value 0x0000, no final XOR, no bit reversal)

**C Implementation**:
```c
uint16_t crc16_ccitt(const uint8_t *buf, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

**C++ Implementation**:
```cpp
uint16_t crc16_ccitt(const std::vector<uint8_t>& data) {
    uint16_t crc = 0x0000;
    for (uint8_t byte : data) {
        crc ^= (uint16_t)byte << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

**CRC is calculated over**: Type (1 byte) + Length (2 bytes) + Payload (N bytes)

---

## Message Types

### Message Type Overview

| Type | Name | Description | Direction | Priority | Max Size |
|------|------|-------------|-----------|----------|----------|
| 0x00 | HEARTBEAT | Keep-alive ping/pong | Both | Low | 4 bytes |
| 0x01 | TIME | Time and date update | Host → ESP32 | Medium | 16 bytes |
| 0x02 | WEATHER | Weather data | Host → ESP32 | Low | 2 KB |
| 0x03 | MEDIA | Media player info | Host → ESP32 | High | 2 KB |
| 0x04 | NOTIFICATION | Notification data | Host → ESP32 | High | 2 KB |
| 0x05 | PORTFOLIO | Portfolio data | Host → ESP32 | Medium | 4 KB |
| 0x06 | SYSTEM | System monitor data | Host → ESP32 | Medium | 512 bytes |
| 0x07 | CALENDAR | Calendar data | Host → ESP32 | Low | 2 KB |
| 0x08 | ACK | Positive acknowledgment | Both | High | 4 bytes |
| 0x09 | NACK | Negative acknowledgment | Both | High | 8 bytes |
| 0x0A | ERROR | Error message | Both | Medium | 256 bytes |
| 0x0B | CONFIG | Configuration exchange | Both | Medium | 256 bytes |
| 0x0C | REQUEST | Data request | ESP32 → Host | Medium | 16 bytes |
| 0x0D | TOUCH | Touch event | ESP32 → Host | High | 16 bytes |
| 0x0E | COMMAND | Command execution | Both | Medium | 256 bytes |
| 0x0F | DEBUG | Debug message | Both | Low | 1 KB |
| 0x10 | FIRMWARE | Firmware update | Host → ESP32 | Low | 4 KB |
| 0x11 | OTA | OTA update chunk | Host → ESP32 | Low | 4 KB |
| 0x12-0x7F | RESERVED | For future use | - | - | - |
| 0x80-0xFF | CUSTOM | Custom messages | Both | - | - |

### Message Type Details

---

## Message Specifications

### 0x00 - HEARTBEAT

**Purpose**: Keep-alive signal to detect connection status

**Direction**: Both

**Payload Structure**:
```c
struct HeartbeatMessage {
    uint8_t sequence;      // Sequence number (increments with each ping)
    uint8_t protocol_major; // Protocol version major
    uint8_t protocol_minor; // Protocol version minor
};
```

**Size**: 3 bytes

**Usage**:
- Host sends HEARTBEAT every 5 seconds
- ESP32 responds with HEARTBEAT within 100ms
- If no response for 3 attempts, connection is considered lost

---

### 0x01 - TIME

**Purpose**: Synchronize time and date

**Direction**: Host → ESP32

**Payload Structure**:
```c
struct TimeMessage {
    uint32_t timestamp;    // Unix timestamp (seconds since 1970-01-01)
    int16_t timezone;      // Timezone offset in minutes from UTC (-720 to 720)
    uint8_t dst_flag;      // Daylight saving time flag (0=off, 1=on)
};
```

**Alternative Binary Format**:
```c
struct TimeMessageBinary {
    uint8_t hour;         // 0-23
    uint8_t minute;       // 0-59
    uint8_t second;       // 0-59
    uint8_t day;          // 1-31
    uint8_t month;        // 1-12
    uint16_t year;        // 2000-2099
    uint8_t day_of_week;  // 0-6 (0=Sunday)
    int16_t timezone;     // Timezone offset in minutes
    uint8_t dst_flag;     // DST flag
};
```

**Size**: 7 or 10 bytes

**Update Frequency**: Every 1 second (for seconds display) or every 1 minute

---

### 0x02 - WEATHER

**Purpose**: Send weather data (current and forecast)

**Direction**: Host → ESP32

**Payload Format**: JSON (recommended for flexibility)
```json
{
  "timestamp": 1633046400,
  "location": "Paris, FR",
  "current": {
    "temperature": 22.5,
    "feels_like": 23.1,
    "humidity": 65,
    "pressure": 1013.25,
    "wind_speed": 12.3,
    "wind_direction": 180,
    "precipitation": 0.0,
    "uv_index": 6,
    "visibility": 10000,
    "condition": "Clear",
    "condition_code": "01d",
    "sunrise": 1633000000,
    "sunset": 1633080000
  },
  "forecast": {
    "hourly": [
      {
        "timestamp": 1633046400,
        "temperature": 22.5,
        "condition": "Clear",
        "condition_code": "01d",
        "precipitation_probability": 0
      },
      // ... more hourly entries
    ],
    "daily": [
      {
        "timestamp": 1633046400,
        "high": 25.0,
        "low": 18.0,
        "condition": "Clear",
        "precipitation_probability": 0
      },
      // ... more daily entries
    ]
  },
  "alerts": [
    {
      "type": "Rain",
      "message": "Light rain expected in 2 hours",
      "severity": "Low"
    }
  ]
}
```

**Binary Format** (more efficient):
```c
struct WeatherCurrent {
    int16_t temperature_x10;     // Temperature * 10 (e.g., 225 = 22.5°C)
    int16_t feels_like_x10;      // Feels like * 10
    uint8_t humidity;            // 0-100%
    uint16_t pressure_x10;       // Pressure * 10 (hPa)
    uint16_t wind_speed_x10;     // Wind speed * 10 (m/s)
    uint16_t wind_direction;     // 0-359 degrees
    uint8_t precipitation_x10;   // Precipitation * 10 (mm)
    uint8_t uv_index;            // 0-11+
    uint16_t visibility;         // Visibility in meters
    uint8_t condition_id;        // Condition code (mapped to icons)
    uint32_t sunrise;            // Unix timestamp
    uint32_t sunset;             // Unix timestamp
};

struct WeatherHourly {
    uint32_t timestamp;
    int16_t temperature_x10;
    uint8_t condition_id;
    uint8_t precipitation_probability;
};

struct WeatherDaily {
    uint32_t timestamp;
    int16_t high_x10;
    int16_t low_x10;
    uint8_t condition_id;
    uint8_t precipitation_probability;
};

struct WeatherMessage {
    uint32_t timestamp;
    uint8_t location_id;         // Predefined location index
    WeatherCurrent current;
    uint8_t hourly_count;
    WeatherHourly hourly[12];    // Next 12 hours
    uint8_t daily_count;
    WeatherDaily daily[5];       // Next 5 days
    uint8_t alert_count;
    // Alerts would follow
};
```

**Size**: ~100-500 bytes (JSON), ~100-200 bytes (binary)

**Update Frequency**: Every 10-30 minutes (configurable)

---

### 0x03 - MEDIA

**Purpose**: Send media player information

**Direction**: Host → ESP32

**Payload Format**: JSON
```json
{
  "player": "Spotify",
  "state": "playing", // "playing", "paused", "stopped"
  "track": {
    "title": "Song Title",
    "artist": "Artist Name",
    "album": "Album Name",
    "duration": 185,    // Duration in seconds
    "position": 45,     // Current position in seconds
    "album_art_url": "https://...",
    "album_art_data": "base64..." // Optional: embedded image
  },
  "volume": 75,        // 0-100
  "repeat": "off",     // "off", "one", "all"
  "shuffle": false,
  "queue": {
    "current_index": 0,
    "length": 50,
    "next": {
      "title": "Next Song",
      "artist": "Next Artist"
    }
  }
}
```

**Binary Format**:
```c
struct MediaTrack {
    char title[64];
    char artist[48];
    char album[48];
    uint32_t duration;          // Seconds
    uint32_t position;          // Seconds
    uint8_t has_album_art;      // 0 or 1
    uint16_t album_art_size;   // If has_album_art
    // Album art data follows if has_album_art
};

struct MediaMessage {
    uint8_t player_id;          // Predefined player index
    uint8_t state;              // 0=stopped, 1=paused, 2=playing
    MediaTrack track;
    uint8_t volume;            // 0-100
    uint8_t repeat_mode;       // 0=off, 1=one, 2=all
    uint8_t shuffle;           // 0 or 1
    uint16_t queue_length;
    // Queue items would follow
};
```

**Size**: ~200-500 bytes (JSON), ~200-256 bytes (binary, without album art)

**Update Frequency**: 
- **State/Metadata**: On change or every 5 seconds
- **Progress**: Every 1 second when playing
- **Volume**: On change

---

### 0x04 - NOTIFICATION

**Purpose**: Send notification data

**Direction**: Host → ESP32

**Payload Format**: JSON
```json
{
  "type": "email", // "email", "app", "system", "calendar"
  "id": "unique_id_123",
  "timestamp": 1633046400,
  "title": "New Email from John",
  "message": "Meeting tomorrow at 10 AM",
  "sender": "john@example.com",
  "priority": "high", // "low", "normal", "high", "urgent"
  "unread": true,
  "account": "Work", // For email notifications
  "actions": ["mark_read", "delete", "reply"]
}
```

**Binary Format**:
```c
struct NotificationMessage {
    uint8_t type;              // 0=email, 1=app, 2=system, 3=calendar
    uint8_t priority;          // 0=low, 1=normal, 2=high, 3=urgent
    uint32_t timestamp;
    uint32_t id_hash;          // Hash of notification ID
    uint8_t unread;           // 0 or 1
    uint8_t title_length;
    char title[64];
    uint8_t message_length;
    char message[128];
    uint8_t sender_length;
    char sender[48];
    // Optional fields follow
};
```

**Size**: ~100-500 bytes

**Update Frequency**: When new notification arrives

---

### 0x05 - PORTFOLIO

**Purpose**: Send portfolio data

**Direction**: Host → ESP32

**Payload Format**: JSON
```json
{
  "timestamp": 1633046400,
  "summary": {
    "total_value": 125432.50,
    "today_change": 456.75,
    "today_change_percent": 0.364,
    "currency": "USD"
  },
  "accounts": [
    {
      "name": "Brokerage",
      "balance": 75000.00,
      "currency": "USD",
      "change": 300.50,
      "change_percent": 0.40
    },
    {
      "name": "Crypto",
      "balance": 12345.67,
      "currency": "USD",
      "change": -250.89,
      "change_percent": -2.0
    }
  ],
  "holdings": [
    {
      "symbol": "AAPL",
      "name": "Apple Inc.",
      "quantity": 10,
      "price": 150.25,
      "value": 1502.50,
      "cost_basis": 1450.00,
      "gain": 52.50,
      "gain_percent": 3.62,
      "weight": 0.012 // 1.2% of portfolio
    },
    // ... more holdings
  ],
  "performance": {
    "today": 0.364,
    "week": 2.15,
    "month": 5.8,
    "quarter": 12.3,
    "year": 25.6,
    "all_time": 156.4
  },
  "options": [
    {
      "symbol": "AAPL221216C00150000",
      "type": "call",
      "strike": 150.00,
      "expiration": "2022-12-16",
      "quantity": 5,
      "price": 2.50,
      "value": 12.50,
      "in_the_money": true,
      "delta": 0.75,
      "gamma": 0.02,
      "theta": -0.01,
      "vega": 0.15
    }
  ]
}
```

**Binary Format**:
```c
struct PortfolioSummary {
    uint64_t total_value_x100;  // Value * 100 (e.g., 12543250 = $125,432.50)
    int32_t today_change_x100;   // Change * 100
    int16_t today_change_pct_x1000; // Percent * 1000 (e.g., 364 = 0.364%)
    uint8_t currency[4];        // "USD", "EUR", etc.
};

struct PortfolioAccount {
    char name[32];
    uint64_t balance_x100;
    uint8_t currency[4];
    int32_t change_x100;
    int16_t change_pct_x1000;
};

struct PortfolioHolding {
    char symbol[16];
    char name[32];
    uint32_t quantity_x1000;     // Quantity * 1000 for fractional shares
    uint32_t price_x100;        // Price * 100
    uint64_t value_x100;
    uint64_t cost_basis_x100;
    int32_t gain_x100;
    int16_t gain_pct_x1000;
    uint16_t weight_x100;       // Weight * 100 (e.g., 120 = 1.2%)
};

struct PortfolioMessage {
    uint32_t timestamp;
    PortfolioSummary summary;
    uint8_t account_count;
    PortfolioAccount accounts[10];
    uint8_t holding_count;
    PortfolioHolding holdings[50];
    // Performance and options data follows
};
```

**Size**: ~500-4000 bytes (depends on number of holdings)

**Update Frequency**: Every 30-60 seconds

---

### 0x06 - SYSTEM

**Purpose**: Send system monitor data

**Direction**: Host → ESP32

**Payload Format**: JSON
```json
{
  "timestamp": 1633046400,
  "cpu": {
    "usage": 25.5,           // Total CPU usage %
    "cores": [
      {"usage": 15.2},
      {"usage": 35.8},
      // ...
    ],
    "temperature": 45.0      // CPU temperature °C
  },
  "memory": {
    "total": 16384,          // Total RAM MB
    "used": 8192,            // Used RAM MB
    "free": 8192,            // Free RAM MB
    "swap_total": 4096,
    "swap_used": 1024
  },
  "storage": [
    {
      "device": "C:",
      "total": 500000,       // Total space MB
      "used": 250000,
      "free": 250000,
      "usage_percent": 50.0
    }
  ],
  "network": {
    "up": 1250,             // Upload speed KB/s
    "down": 5000,           // Download speed KB/s
    "total_up": 102400,     // Total uploaded MB
    "total_down": 512000    // Total downloaded MB
  },
  "gpu": {
    "usage": 45.0,           // GPU usage %
    "temperature": 65.0,    // GPU temperature °C
    "vram_total": 8192,     // Total VRAM MB
    "vram_used": 4096       // Used VRAM MB
  }
}
```

**Binary Format**:
```c
struct SystemMessage {
    uint32_t timestamp;
    
    // CPU
    uint16_t cpu_usage_x10;    // CPU usage * 10 (0-1000 = 0-100%)
    int16_t cpu_temp_x10;     // Temperature * 10
    uint8_t core_count;
    uint16_t core_usages[8];   // Each * 10
    
    // Memory
    uint32_t mem_total;        // Total RAM KB
    uint32_t mem_used;         // Used RAM KB
    uint32_t swap_total;      // Total swap KB
    uint32_t swap_used;       // Used swap KB
    
    // Storage
    uint8_t storage_count;
    struct {
        uint8_t device[8];     // "C:", "D:", etc.
        uint32_t total;         // Total space MB
        uint32_t used;          // Used space MB
    } storage[8];
    
    // Network
    uint32_t net_up;           // Upload speed KB/s
    uint32_t net_down;         // Download speed KB/s
    uint32_t net_total_up;    // Total uploaded MB
    uint32_t net_total_down;  // Total downloaded MB
    
    // GPU (optional)
    uint16_t gpu_usage_x10;   // GPU usage * 10
    int16_t gpu_temp_x10;     // Temperature * 10
    uint32_t vram_total;      // Total VRAM MB
    uint32_t vram_used;       // Used VRAM MB
};
```

**Size**: ~100-200 bytes

**Update Frequency**: Every 2-5 seconds

---

### 0x07 - CALENDAR

**Purpose**: Send calendar event data

**Direction**: Host → ESP32

**Payload Format**: JSON
```json
{
  "timestamp": 1633046400,
  "date": "2022-10-01",
  "events": [
    {
      "id": "event_123",
      "title": "Team Meeting",
      "start": "2022-10-01T10:00:00",
      "end": "2022-10-01T11:00:00",
      "location": "Conference Room A",
      "description": "Weekly team sync",
      "organizer": "manager@example.com",
      "calendar": "Work",
      "recurring": true,
      "reminder": 15 // Reminder in minutes
    },
    // ... more events
  ],
  "upcoming": [
    {
      "title": "Lunch with John",
      "start": "2022-10-01T12:00:00",
      "time_until": 90 // Minutes until event
    }
  ]
}
```

**Binary Format**:
```c
struct CalendarEvent {
    char id[16];
    char title[64];
    uint32_t start_timestamp;
    uint32_t end_timestamp;
    char location[48];
    uint8_t recurring;
    uint8_t reminder_minutes;
    uint8_t calendar_id;
};

struct CalendarMessage {
    uint32_t timestamp;
    uint8_t event_count;
    CalendarEvent events[20];
    uint8_t upcoming_count;
    // Upcoming events would follow
};
```

**Size**: ~200-2000 bytes

**Update Frequency**: Every 5-15 minutes

---

### 0x08 - ACK (Acknowledgment)

**Purpose**: Positive acknowledgment of message receipt

**Direction**: Both

**Payload Structure**:
```c
struct AckMessage {
    uint8_t acked_type;   // Type of message being acknowledged
    uint8_t sequence;     // Sequence number (for ordered messages)
};
```

**Size**: 2 bytes

**Usage**: 
- Sent in response to most message types
- Includes the type of the acknowledged message
- Optional sequence number for ordered protocols

---

### 0x09 - NACK (Negative Acknowledgment)

**Purpose**: Indicate message was received but could not be processed

**Direction**: Both

**Payload Structure**:
```c
struct NackMessage {
    uint8_t nacked_type;     // Type of message that failed
    uint8_t error_code;      // Error code (see Error Codes section)
    uint8_t sequence;        // Sequence number
};
```

**Size**: 3 bytes

---

### 0x0A - ERROR

**Purpose**: Send error information

**Direction**: Both

**Payload Format**: JSON
```json
{
  "code": 2,
  "message": "CRC mismatch",
  "details": "Expected CRC: 0x1234, Received: 0x5678",
  "severity": "error", // "info", "warning", "error", "critical"
  "timestamp": 1633046400
}
```

**Binary Format**:
```c
struct ErrorMessage {
    uint8_t code;
    uint8_t severity;      // 0=info, 1=warning, 2=error, 3=critical
    uint32_t timestamp;
    uint8_t message_length;
    char message[64];
};
```

**Size**: ~10-100 bytes

---

### 0x0B - CONFIG

**Purpose**: Exchange configuration information

**Direction**: Both

**Payload Format**: JSON
```json
{
  "version": {
    "protocol_major": 1,
    "protocol_minor": 0,
    "firmware_version": "1.2.3",
    "server_version": "1.0.0"
  },
  "capabilities": {
    "features": ["time", "weather", "media", "notifications"],
    "max_payload_size": 4096,
    "supported_baud_rates": [115200, 230400, 460800, 921600]
  },
  "settings": {
    "update_intervals": {
      "time": 1000,
      "weather": 600000,
      "media": 1000,
      "portfolio": 30000
    },
    "display": {
      "brightness": 100,
      "timeout": 30000
    }
  }
}
```

**Binary Format**:
```c
struct ConfigMessage {
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint8_t firmware_version[8];
    uint8_t server_version[8];
    uint32_t capabilities;     // Bitmask of supported features
    uint16_t max_payload_size;
    uint32_t supported_baud_rates[4];
    // Settings would follow
};
```

**Size**: ~50-256 bytes

---

### 0x0C - REQUEST

**Purpose**: Request specific data from the other side

**Direction**: ESP32 → Host (typically)

**Payload Structure**:
```c
struct RequestMessage {
    uint8_t request_type;   // What to request (e.g., 0x01 = time sync)
    uint8_t parameter;      // Additional parameter
};
```

**Size**: 2 bytes

**Request Types**:
- 0x00: Full sync (all data)
- 0x01: Time sync
- 0x02: Weather update
- 0x03: Media update
- 0x04: Notification list
- 0x05: Portfolio update
- 0x06: System monitor update
- 0x07: Calendar update
- 0x08: Configuration
- 0x09: Reboot ESP32
- 0x0A: Factory reset

---

### 0x0D - TOUCH

**Purpose**: Send touch event data

**Direction**: ESP32 → Host

**Payload Structure**:
```c
struct TouchPoint {
    uint16_t x;             // X coordinate (0-479)
    uint16_t y;             // Y coordinate (0-479)
    uint8_t pressure;       // Pressure (0-255)
};

struct TouchMessage {
    uint8_t event_type;     // 0=down, 1=up, 2=move, 3=long_press
    uint8_t tap_count;      // Number of taps (1, 2, 3...)
    TouchPoint point;
    uint32_t timestamp;     // Timestamp of event (ms since boot)
};
```

**Size**: 9 bytes

**Event Types**:
- **DOWN** (0): Finger touched screen
- **UP** (1): Finger lifted from screen
- **MOVE** (2): Finger moved on screen
- **LONG_PRESS** (3): Long press detected (typically >500ms)
- **DOUBLE_TAP** (4): Double tap detected
- **SWIPE_LEFT** (5): Swipe left gesture
- **SWIPE_RIGHT** (6): Swipe right gesture
- **SWIPE_UP** (7): Swipe up gesture
- **SWIPE_DOWN** (8): Swipe down gesture
- **PINCH_IN** (9): Pinch in gesture
- **PINCH_OUT** (10): Pinch out gesture

---

### 0x0E - COMMAND

**Purpose**: Execute a command

**Direction**: Both

**Payload Format**: JSON
```json
{
  "command": "set_brightness",
  "parameters": {
    "value": 75
  },
  "request_ack": true
}
```

**Binary Format**:
```c
struct CommandMessage {
    uint8_t command_id;     // Command identifier
    uint8_t param_count;    // Number of parameters
    // Parameters follow as key-value pairs
};
```

**Command List**:

**Display Commands (ESP32)**:
- `set_brightness` (value: 0-100)
- `set_timeout` (seconds: 0-3600, 0=never)
- `set_orientation` (0=portrait, 1=landscape)
- `set_theme` (theme_id: 0-255)
- `screenshot` (capture and send display buffer)
- `reboot` (reboot ESP32)
- `factory_reset` (reset to factory settings)

**Media Commands (Host)**:
- `play_pause` (toggle play/pause)
- `next` (next track)
- `previous` (previous track)
- `set_volume` (value: 0-100)
- `seek` (position: seconds)
- `set_repeat` (mode: off/one/all)
- `set_shuffle` (enabled: true/false)

**System Commands (Host)**:
- `sleep` (sleep computer)
- `shutdown` (shutdown computer)
- `restart` (restart computer)
- `open_app` (app: string)
- `run_script` (script: string)

---

### 0x0F - DEBUG

**Purpose**: Send debug information

**Direction**: Both

**Payload Format**: Text
```
[DEBUG] [SERIAL] Received message type 0x01, length 16
[WARN] [PROTOCOL] CRC mismatch: expected 0x1234, got 0x5678
[ERROR] [MEMORY] Heap overflow: requested 1024 bytes, only 512 free
```

**Binary Format**:
```c
struct DebugMessage {
    uint8_t level;            // 0=verbose, 1=debug, 2=info, 3=warn, 4=error
    uint8_t source_length;
    char source[16];         // Source module name
    uint16_t message_length;
    char message[];          // Variable length message
};
```

---

## Error Codes

| Code | Name | Description | Recovery |
|------|------|-------------|----------|
| 0x00 | SUCCESS | Operation successful | - |
| 0x01 | INVALID_FORMAT | Invalid message format | Resend with correct format |
| 0x02 | CRC_ERROR | CRC checksum mismatch | Resend message |
| 0x03 | TIMEOUT | Operation timed out | Retry or abort |
| 0x04 | BUFFER_OVERFLOW | Buffer overflow | Increase buffer size, reduce message size |
| 0x05 | NOT_IMPLEMENTED | Feature not implemented | Check protocol version |
| 0x06 | INVALID_STATE | Invalid state for operation | Wait for valid state |
| 0x07 | RESOURCE_ERROR | Resource unavailable | Retry later |
| 0x08 | COMMUNICATION_ERROR | Serial communication error | Reinitialize connection |
| 0x09 | PARSING_ERROR | Data parsing error | Check data format |
| 0x0A | MEMORY_ERROR | Memory allocation error | Free memory, retry |
| 0x0B | PROTOCOL_VERSION_MISMATCH | Protocol version mismatch | Negotiate compatible version |
| 0x0C | MESSAGE_TOO_LARGE | Message exceeds max size | Split message or reduce data |
| 0x0D | INVALID_TYPE | Unknown message type | Ignore or return error |
| 0x0E | CHECKSUM_ERROR | Data integrity error | Resend data |
| 0x0F | HARDWARE_ERROR | Hardware failure | User intervention required |
| 0x10-0x7F | RESERVED | Reserved for future use | - |
| 0x80-0xFF | CUSTOM | Custom error codes | Application-specific |

---

## Protocol State Machine

### Connection States
```
                    +----------+
                    |   IDLE   |
                    +----+-----+
                         | Reset/Timeout
                         v
                    +----+-----+
          +---------| INITIAL |----------+
          |         +----+-----+          |
          |              |                |
          |    Handshake |                |
          |      Success |                |
          v              v                |
    +-----+------+ +----+-----+          |
    |  FAILED    | | CONNECTED|          |
    +-----+------+ +----+-----+          |
          ^              |                |
          |              +----v-----+     |
          |                     |          |
          |              +----+-----+     |
          |              | DISCONNECT|     |
          |              |    ED    |     |
          |              +------------+     |
          |                             |
          +-----------------------------+
```

### State Descriptions

#### IDLE
- No active connection
- Serial port not initialized or closed
- ESP32: Waiting for serial initialization
- Host: Waiting for connection request

**Transitions**:
- On serial initialization → INITIAL

#### INITIAL
- Connection being established
- Performing handshake
- Exchanging protocol versions and capabilities

**Actions**:
1. Host sends HEARTBEAT (ping)
2. ESP32 responds with HEARTBEAT + CONFIG
3. Host validates protocol version
4. Host sends CONFIG + initial data (TIME)
5. ESP32 acknowledges

**Transitions**:
- Handshake success → CONNECTED
- Handshake failure → FAILED
- Timeout (5 seconds) → IDLE

#### CONNECTED
- Normal operation
- Messages flowing in both directions
- Periodic heartbeats

**Actions**:
1. Exchange data messages
2. Send heartbeats every 5 seconds
3. Acknowledge received messages
4. Process touch events

**Transitions**:
- No heartbeat for 15 seconds → DISCONNECTED
- Protocol error → DISCONNECTED
- User request → DISCONNECTED

#### DISCONNECTED
- Connection lost or closed
- Attempting to reconnect

**Actions**:
1. Attempt reconnection every 2 seconds
2. After 10 attempts, wait 30 seconds before retrying
3. On successful reconnection → INITIAL

**Transitions**:
- Reconnection successful → INITIAL
- Max retry attempts → FAILED

#### FAILED
- Connection failed permanently
- Requires manual intervention

**Actions**:
1. Log error
2. Display error on dashboard
3. Wait for user action or system restart

**Transitions**:
- User initiates reconnect → INITIAL
- System restart → IDLE

---

## Message Flow Examples

### Normal Data Exchange
```
Host:                        ESP32:
  |                          |
  +-- [0x01 TIME] ----------->|
  |                          |
  |<-- [0x08 ACK] -----------+
  |                          |
  +-- [0x03 MEDIA] --------->|
  |                          |
  |<-- [0x08 ACK] -----------+
  |                          |
  +-- [0x02 WEATHER] -------->|
  |                          |
  |<-- [0x08 ACK] -----------+
```

### Error Handling
```
Host:                        ESP32:
  |                          |
  +-- [0x02 WEATHER] -------->|
  |                          |
  |<-- [0x09 NACK:0x02] ------+  (CRC error)
  |                          |
  +-- [0x02 WEATHER] -------->|  (Retry)
  |                          |
  |<-- [0x08 ACK] -----------+
```

### Touch Event
```
Host:                        ESP32:
  |                          |
  |<-- [0x0D TOUCH:DOWN] ----+
  |                          |
  +-- [0x08 ACK] ----------->|
  |                          |
  |<-- [0x0D TOUCH:MOVE] ---+
  |                          |
  |<-- [0x0D TOUCH:UP] -----+
```

### Command Execution
```
Host:                        ESP32:
  |                          |
  +-- [0x0E COMMAND:play] -->|
  |                          |
  |<-- [0x08 ACK] -----------+  (Command received)
  |                          |
  |<-- [0x03 MEDIA:playing] +  (State update)
```

### Connection Loss and Recovery
```
Host:                        ESP32:
  |                          |
  +-- [0x00 HEARTBEAT] ------>|
  |                          |
  |  (No response - timeout)   |
  |                          |
  +-- [0x00 HEARTBEAT] ------>|
  |                          |
  |  (No response - timeout)   |
  |                          |
  +-- [0x00 HEARTBEAT] ------>|
  |                          |
  |  (No response - timeout)   |
  |                          |
  |  [Connection lost]          |
  |                          |
  |  [Attempt reconnect...]    |
  |                          |
```

---

## Performance Optimization

### ESP32 CPU Prioritization

**Core Principle**: The ESP32's primary job is to maintain a **responsive UI**. All protocol design decisions must prioritize keeping CPU usage low enough that the display remains smooth (60 FPS) and touch events are processed without lag.

**ESP32 Constraints**:
- **CPU**: Dual-core Xtensa LX6 at 160-240 MHz
- **Single-core mode**: Typically ~160 MHz for most operations
- **RAM**: ~520 KB (varies by board variant)
- **Flash**: 4+ MB for code and data
- **Real-time requirements**: 
  - UI rendering: ~16ms per frame (60 FPS)
  - Touch sampling: < 10ms response time
  - Serial processing: Must not block for > 1ms

**CPU Usage Budget**:
```
Total CPU time per 16ms frame: 16ms
├── Display rendering: ~5-8ms (LVGL)
├── Touch processing: ~1-2ms
├── Serial I/O: ~1-2ms
├── Protocol processing: ~1-2ms (target)
└── Available for compression: ~2-4ms (absolute maximum)

Recommendation: Keep protocol processing < 2ms per message
```

**Design Guidelines**:
1. **Minimize blocking operations**: Use non-blocking I/O
2. **Avoid long-running computations**: Break into chunks
3. **Use efficient algorithms**: Prefer speed over perfect compression
4. **Cache frequently used data**: Avoid repeated calculations
5. **Batch operations**: Group multiple updates into single messages
6. **Prioritize critical tasks**: UI and touch > serial processing

**Monitoring CPU Usage**:
```cpp
// ESP32: Check CPU usage
uint32_t start_time = micros();
// ... perform operation ...
uint32_t elapsed = micros() - start_time;

// Log if too slow
if (elapsed > 2000) { // > 2ms
    logWarning("Operation took %lu us", elapsed);
}

// Monitor loop execution time
static uint32_t max_loop_time = 0;
uint32_t loop_start = micros();
// ... main loop ...
uint32_t loop_time = micros() - loop_start;
max_loop_time = std::max(max_loop_time, loop_time);

// If loop time > 16ms, we're missing frames
if (loop_time > 16000) {
    logError("Frame time exceeded: %lu us", loop_time);
}
```

### Message Batching

To reduce overhead, multiple data updates can be batched into a single message:

**Example: Batched Update**
```json
{
  "batch": true,
  "updates": [
    {"type": "time", "data": {...}},
    {"type": "media", "data": {...}},
    {"type": "notification", "data": {...}}
  ]
}
```

### Delta Updates

Send only changed values to minimize data transfer:

**Example: Media Delta Update**
```json
{
  "type": "media",
  "delta": true,
  "changes": {
    "position": 46,        // Only position changed
    "state": "playing"     // State unchanged
  }
}
```

### Binary vs JSON

| Format | Pros | Cons | Best For |
|--------|------|------|----------|
| **JSON** | Human-readable, flexible, easy to debug | Larger size, slower parsing | Development, debugging, low-frequency data |
| **Binary** | Compact, fast parsing, efficient | Not human-readable, requires schema | Production, high-frequency data, large datasets |

**Recommendation**: Use JSON for development and debugging, switch to binary for production.

**Constant Definition**: For easy switching between modes, define constants in both firmware and host:

```cpp
// In protocol.hpp (shared header or duplicated in both projects)

// Message format mode - SINGLE CONSTANT TO CHANGE FOR ALL MESSAGES
#define MESSAGE_FORMAT_JSON  0   // Human-readable, for development
#define MESSAGE_FORMAT_BINARY 1  // Efficient, for production

// Current active format (change this one line to switch all messages)
#define MESSAGE_FORMAT MESSAGE_FORMAT_JSON  // or MESSAGE_FORMAT_BINARY

// Helper macros
#if MESSAGE_FORMAT == MESSAGE_FORMAT_JSON
    #define USE_JSON_FORMAT 1
#else
    #define USE_JSON_FORMAT 0
#endif
```

**Usage in code**:
```cpp
// When sending a message
#if USE_JSON_FORMAT
    // Serialize to JSON
    std::string json_payload = serializeToJson(data);
    sendMessage(type, reinterpret_cast<const uint8_t*>(json_payload.data()), json_payload.size());
#else
    // Serialize to binary
    uint8_t binary_payload[256];
    size_t payload_size = serializeToBinary(data, binary_payload, sizeof(binary_payload));
    sendMessage(type, binary_payload, payload_size);
#endif

// When receiving a message
#if USE_JSON_FORMAT
    // Parse JSON
    auto data = parseFromJson(payload);
#else
    // Parse binary
    auto data = parseFromBinary(payload, length);
#endif
```

### Compression

**Design Priority**: ESP32 CPU usage minimization for maximum responsiveness.

For large payloads (portfolio data with many holdings), compression can be applied. However, **CPU usage on the ESP32 must be prioritized** to maintain smooth UI rendering and touch responsiveness.

**Recommended Compression**: **LZ4** (Primary)

| Algorithm | Compression Ratio | Speed | CPU Usage | RAM Usage | Best For |
|-----------|------------------|-------|-----------|-----------|----------|
| **None** | 1.0x | ⚡⚡⚡⚡⚡ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | Small payloads, real-time data |
| **LZ4** | ~2.0-3.0x | ⚡⚡⚡⚡ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | **Large payloads, ESP32 recommended** |
| **LZ4HC** | ~2.5-3.5x | ⚡⚡⚡ | ⭐⭐⭐⭐ | ⭐⭐⭐ | When compression ratio > speed priority |
| **Zlib** | ~3.0-4.0x | ⚡⚡ | ⭐⭐⭐ | ⭐⭐ | Not recommended for ESP32 |
| **Gzip** | ~3.5-4.5x | ⚡ | ⭐⭐ | ⭐ | Not suitable for ESP32 |

**LZ4 is the best choice** for this project because:
1. **Extremely fast compression**: ~400 MB/s on modern CPUs, still very fast on ESP32
2. **Ultra-fast decompression**: ~2 GB/s, negligible overhead on ESP32
3. **Low CPU usage**: Minimal impact on ESP32 responsiveness
4. **Low memory usage**: Small code footprint, minimal RAM
5. **Available for ESP32**: [LZ4 library for Arduino/ESP32](https://github.com/ComplexMoney/lz4-esp32)
6. **Acceptable compression ratio**: ~2-3x reduction for JSON/text data

**Implementation**:

**ESP32 Side (with LZ4 library)**:
```cpp
// Add to platformio.ini:
// lib_deps =
//     https://github.com/ComplexMoney/lz4-esp32.git

#include <lz4.h>

// Compress payload before sending
std::vector<uint8_t> compressPayload(const std::vector<uint8_t>& input) {
    // Maximum compressed size (LZ4 can expand by 0.4% in worst case)
    size_t max_compressed_size = LZ4_compressBound(input.size());
    std::vector<uint8_t> compressed(max_compressed_size);
    
    // Compress
    int compressed_size = LZ4_compress_default(
        reinterpret_cast<const char*>(input.data()),
        reinterpret_cast<char*>(compressed.data()),
        input.size(),
        max_compressed_size
    );
    
    if (compressed_size <= 0) {
        // Compression failed, send uncompressed
        return input;
    }
    
    // Resize to actual compressed size
    compressed.resize(compressed_size);
    return compressed;
}

// Decompress received payload
std::vector<uint8_t> decompressPayload(const std::vector<uint8_t>& compressed) {
    // Get original size (should be sent with compressed data)
    // For this example, assume we have a way to get original_size
    
    std::vector<uint8_t> decompressed(original_size);
    
    int decompressed_size = LZ4_decompress_safe(
        reinterpret_cast<const char*>(compressed.data()),
        reinterpret_cast<char*>(decompressed.data()),
        compressed.size(),
        original_size
    );
    
    if (decompressed_size <= 0) {
        // Decompression failed
        return {}; // Return empty vector
    }
    
    decompressed.resize(decompressed_size);
    return decompressed;
}
```

**Modified Message Structure with Compression Flag**:
```
+--------+--------+--------+--------+--------+--------+--------+
| SOF    | Type   | Length | Flags  | Payload| CRC    | EOF    |
| 1 byte | 1 byte | 2 bytes| 1 byte | N bytes| 2 bytes| 1 byte |
+--------+--------+--------+--------+--------+--------+--------+
```

**Flags Byte** (bit fields):
- Bit 0: Compression (0 = none, 1 = LZ4)
- Bits 1-7: Reserved for future use

**ESP32 CPU Usage Guidelines**:

1. **Always prefer uncompressed** for small messages (< 256 bytes)
   - Compression overhead > size savings
   
2. **Use LZ4 compression** for large messages (> 512 bytes)
   - Portfolio data with many holdings
   - Weather data with extended forecasts
   - Batch updates with multiple data types
   
3. **Avoid compression** for real-time data:
   - Touch events
   - Heartbeats
   - Time updates
   - Media progress updates
   
4. **Benchmark on target hardware**:
   - Measure compression/decompression times
   - Monitor frame rates during compression
   - Ensure UI remains responsive

5. **Fallback mechanism**:
   - If compression takes > 5ms, send uncompressed
   - If decompression takes > 5ms, request uncompressed
   - Log compression performance for optimization

**Performance Testing Results** (typical ESP32):
```
| Payload Size | Uncompressed | LZ4 Compressed | Compression Time | Decompression Time |
|--------------|--------------|----------------|-----------------|-------------------|
| 256 bytes    | 256 bytes     | ~180 bytes      | ~0.5ms           | ~0.2ms            |
| 512 bytes    | 512 bytes     | ~320 bytes      | ~1.0ms           | ~0.4ms            |
| 1 KB         | 1024 bytes    | ~450 bytes      | ~1.8ms           | ~0.7ms            |
| 2 KB         | 2048 bytes    | ~850 bytes      | ~3.5ms           | ~1.3ms            |
| 4 KB         | 4096 bytes    | ~1500 bytes     | ~7.0ms           | ~2.5ms            |
```

**Conclusion**: LZ4 compression adds minimal overhead (< 10ms for even the largest payloads) while providing ~2-3x size reduction. This is an excellent trade-off for maintaining ESP32 responsiveness.

---

## Implementation Guidelines

### ESP32 Side Implementation

#### Serial Initialization
```cpp
// main.cpp
#include <HardwareSerial.h>

HardwareSerial SerialPort(0); // UART0

void setup() {
    SerialPort.begin(115200, SERIAL_8N1, 1, 3); // RX=1, TX=3
    SerialPort.setTimeout(100);
    
    // Set up serial buffer
    serialBuffer.reserve(SERIAL_BUFFER_SIZE);
    
    // Set up serial interrupt (optional)
    SerialPort.onReceive(onSerialReceive, true);
}
```

#### Message Processing
```cpp
// protocol.cpp
void processSerialData() {
    while (SerialPort.available()) {
        uint8_t byte = SerialPort.read();
        
        // Handle byte stuffing
        if (byte == ESC) {
            if (SerialPort.available()) {
                byte = SerialPort.read() ^ 0xA5;
            }
        }
        
        // State machine for message parsing
        static enum { WAIT_SOF, READ_TYPE, READ_LENGTH, READ_PAYLOAD, READ_CRC, READ_EOF } state = WAIT_SOF;
        static uint8_t type;
        static uint16_t length;
        static uint16_t index;
        static uint16_t crc;
        static uint8_t buffer[PAYLOAD_BUFFER_SIZE];
        
        switch (state) {
            case WAIT_SOF:
                if (byte == SOF) state = READ_TYPE;
                break;
                
            case READ_TYPE:
                type = byte;
                state = READ_LENGTH;
                index = 0;
                break;
                
            case READ_LENGTH:
                if (index == 0) {
                    length = byte << 8;
                } else {
                    length |= byte;
                    if (length > PAYLOAD_BUFFER_SIZE) {
                        state = WAIT_SOF; // Reset on error
                        break;
                    }
                    state = READ_PAYLOAD;
                    index = 0;
                    crc = crc16_update(0, type);
                    crc = crc16_update(crc, length >> 8);
                    crc = crc16_update(crc, length & 0xFF);
                }
                index++;
                break;
                
            case READ_PAYLOAD:
                buffer[index] = byte;
                crc = crc16_update(crc, byte);
                index++;
                if (index >= length) {
                    state = READ_CRC;
                    index = 0;
                }
                break;
                
            case READ_CRC:
                if (index == 0) {
                    received_crc = byte << 8;
                } else {
                    received_crc |= byte;
                    state = READ_EOF;
                }
                index++;
                break;
                
            case READ_EOF:
                if (byte == EOF) {
                    // Message complete
                    if (crc == received_crc) {
                        handleMessage(type, buffer, length);
                    } else {
                        sendNack(type, ERROR_CRC_ERROR);
                    }
                }
                state = WAIT_SOF; // Reset
                break;
        }
    }
}
```

#### Sending Messages
```cpp
// protocol.cpp
void sendMessage(uint8_t type, const uint8_t* payload, size_t length) {
    uint8_t frame[FRAME_BUFFER_SIZE];
    size_t frame_index = 0;
    
    // SOF
    frame[frame_index++] = SOF;
    
    // Type
    frame[frame_index++] = type;
    
    // Length (big-endian)
    frame[frame_index++] = length >> 8;
    frame[frame_index++] = length & 0xFF;
    
    // Calculate CRC
    uint16_t crc = crc16_ccitt(&type, 1);
    crc = crc16_update(crc, length >> 8);
    crc = crc16_update(crc, length & 0xFF);
    for (size_t i = 0; i < length; i++) {
        crc = crc16_update(crc, payload[i]);
    }
    
    // Copy payload with byte stuffing
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = payload[i];
        if (byte == SOF || byte == EOF || byte == ESC) {
            frame[frame_index++] = ESC;
            frame[frame_index++] = byte ^ 0xA5;
        } else {
            frame[frame_index++] = byte;
        }
    }
    
    // CRC (big-endian)
    frame[frame_index++] = crc >> 8;
    frame[frame_index++] = crc & 0xFF;
    
    // EOF
    frame[frame_index++] = EOF;
    
    // Send frame
    SerialPort.write(frame, frame_index);
}
```

### Host Side Implementation

#### Serial Port Setup (Windows)
```cpp
// serial_port.cpp (Windows)
#include <windows.h>
#include <vector>

class SerialPort {
public:
    SerialPort(const std::string& port_name, uint32_t baud_rate) {
        hSerial = CreateFileA(port_name.c_str(),
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
        
        if (hSerial == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open serial port");
        }
        
        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        
        if (!GetCommState(hSerial, &dcbSerialParams)) {
            CloseHandle(hSerial);
            throw std::runtime_error("Failed to get serial port state");
        }
        
        dcbSerialParams.BaudRate = baud_rate;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        
        if (!SetCommState(hSerial, &dcbSerialParams)) {
            CloseHandle(hSerial);
            throw std::runtime_error("Failed to set serial port state");
        }
        
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;
        
        if (!SetCommTimeouts(hSerial, &timeouts)) {
            CloseHandle(hSerial);
            throw std::runtime_error("Failed to set serial port timeouts");
        }
    }
    
    ~SerialPort() {
        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
        }
    }
    
    size_t read(uint8_t* buffer, size_t size) {
        DWORD bytes_read = 0;
        ReadFile(hSerial, buffer, size, &bytes_read, NULL);
        return bytes_read;
    }
    
    void write(const uint8_t* data, size_t size) {
        DWORD bytes_written = 0;
        WriteFile(hSerial, data, size, &bytes_written, NULL);
    }
    
private:
    HANDLE hSerial;
};
```

#### Message Processing
```cpp
// protocol_handler.cpp
class ProtocolHandler {
public:
    void processReceivedData(const std::vector<uint8_t>& data) {
        size_t pos = 0;
        
        while (pos < data.size()) {
            // Find SOF
            size_t sof_pos = findByte(data, pos, SOF);
            if (sof_pos == std::string::npos) break;
            
            pos = sof_pos + 1;
            
            // Need at least: Type(1) + Length(2) + CRC(2) + EOF(1) = 6 bytes
            if (pos + 5 >= data.size()) break;
            
            // Read type and length
            uint8_t type = data[pos++];
            uint16_t length = (data[pos] << 8) | data[pos + 1];
            pos += 2;
            
            // Check if we have complete message
            size_t message_end = pos + length + 3; // payload + CRC + EOF
            if (message_end > data.size()) break;
            
            // Extract payload
            std::vector<uint8_t> payload(data.begin() + pos, data.begin() + pos + length);
            pos += length;
            
            // Read CRC
            uint16_t received_crc = (data[pos] << 8) | data[pos + 1];
            pos += 2;
            
            // Read EOF
            if (data[pos++] != EOF) {
                // Invalid frame
                continue;
            }
            
            // Calculate CRC
            std::vector<uint8_t> crc_data;
            crc_data.push_back(type);
            crc_data.push_back(length >> 8);
            crc_data.push_back(length & 0xFF);
            crc_data.insert(crc_data.end(), payload.begin(), payload.end());
            uint16_t calculated_crc = crc16_ccitt(crc_data);
            
            // Verify CRC
            if (calculated_crc == received_crc) {
                handleMessage(type, payload);
            } else {
                // CRC error
                sendNack(type, ERROR_CRC_ERROR);
            }
        }
    }
    
private:
    void handleMessage(uint8_t type, const std::vector<uint8_t>& payload) {
        switch (type) {
            case MSG_HEARTBEAT:
                handleHeartbeat(payload);
                break;
            case MSG_TOUCH:
                handleTouchEvent(payload);
                break;
            case MSG_REQUEST:
                handleRequest(payload);
                break;
            // ... other message types
        }
    }
    
    size_t findByte(const std::vector<uint8_t>& data, size_t start, uint8_t byte) {
        for (size_t i = start; i < data.size(); i++) {
            if (data[i] == byte) return i;
        }
        return std::string::npos;
    }
};
```

---

## Testing the Protocol

### Test Cases

#### Basic Communication
1. **Heartbeat**: Verify ping-pong works
2. **Simple Message**: Send and receive a simple message
3. **Large Message**: Send maximum size message
4. **Fragmented Message**: Send message split across multiple reads

#### Error Handling
1. **CRC Error**: Send message with invalid CRC
2. **Invalid SOF**: Send message with wrong SOF
3. **Invalid EOF**: Send message with wrong EOF
4. **Byte Stuffing**: Send message containing SOF, EOF, ESC bytes
5. **Buffer Overflow**: Send message larger than buffer

#### Stress Testing
1. **High Frequency**: Send messages at maximum rate
2. **Concurrent**: Send multiple message types simultaneously
3. **Long Duration**: Run for 24+ hours without errors

#### Protocol Testing
1. **Version Negotiation**: Test with different protocol versions
2. **Fallback**: Test fallback to compatible version
3. **State Transitions**: Test all state machine transitions

### Test Tools

#### Loopback Test
```cpp
// ESP32 loopback test
void loopbackTest() {
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0xAA, 0x55, 0x5A};
    
    for (int i = 0; i < 1000; i++) {
        sendMessage(MSG_DEBUG, test_data, sizeof(test_data));
        
        // Wait for echo
        uint32_t start = millis();
        while (millis() - start < 100) {
            if (SerialPort.available()) {
                uint8_t received;
                SerialPort.readBytes(&received, 1);
                // Verify...
            }
        }
    }
}
```

#### Throughput Test
```cpp
// Measure throughput
void throughputTest() {
    uint32_t start = millis();
    size_t bytes_sent = 0;
    
    for (int i = 0; i < 10000; i++) {
        uint8_t data[256];
        memset(data, i % 256, sizeof(data));
        sendMessage(MSG_DEBUG, data, sizeof(data));
        bytes_sent += sizeof(data) + 6; // + frame overhead
    }
    
    uint32_t elapsed = millis() - start;
    float throughput = (bytes_sent * 1000.0) / (elapsed * 1024.0); // KB/s
    
    Serial.printf("Throughput: %.2f KB/s\n", throughput);
}
```

---

## Protocol Evolution

### Version Compatibility

**Rules**:
1. **Major Version Changes** (e.g., 1.x → 2.0):
   - Breaking changes allowed
   - New major version is not compatible with old minor versions
   - Requires both sides to update

2. **Minor Version Changes** (e.g., 1.0 → 1.1):
   - Backward compatible
   - New features can be added
   - Old implementations can ignore new features
   - New implementations must support old features

3. **Patch Version Changes** (e.g., 1.0.0 → 1.0.1):
   - Bug fixes only
   - No new features or breaking changes

### Version Negotiation

During handshake:
1. Both sides send their protocol version (major.minor)
2. Each side checks if it can communicate with the other
3. If major versions match, communication is possible
4. If major versions don't match, use fallback protocol or fail

**Example**:
- Host: v1.2
- ESP32: v1.1
- Result: Compatible (minor version difference)

- Host: v1.2
- ESP32: v2.0
- Result: Incompatible (major version difference)

### Fallback Protocol

If versions are incompatible but a fallback is available:
1. Use the lowest common major version
2. Use the lowest common minor version within that major version

**Example**:
- Host supports: v1.0, v1.1, v1.2, v2.0
- ESP32 supports: v1.0, v1.1, v2.0
- Common: v1.0, v1.1, v2.0
- Fallback: v1.1 (highest common)

---

## Security Considerations

### Message Validation
1. **Length Validation**: Check that length is within valid range
2. **CRC Verification**: Always verify CRC before processing
3. **Type Validation**: Check that message type is valid
4. **Buffer Bounds**: Ensure payload doesn't overflow buffers

### Input Sanitization
1. **String Lengths**: Limit string lengths in JSON parsing
2. **Numeric Ranges**: Validate numeric values are in expected ranges
3. **Buffer Sizes**: Ensure all buffers are properly sized

### Error Recovery
1. **Reset on Error**: On fatal errors, reset connection
2. **Limit Retries**: Don't retry indefinitely
3. **Fallback Mode**: If protocol fails, enter fallback mode

---

## Appendix

### Quick Reference

| Item | Value |
|------|-------|
| SOF | 0xAA |
| EOF | 0x55 |
| ESC | 0x5A |
| Default Baud Rate | 115200 |
| Max Payload | 4095 bytes |
| CRC Algorithm | CRC-16/CCITT (0x1021) |
| Current Protocol Version | 1.0 |

### Message Type Summary

| Type | Name | Direction | Size |
|------|------|-----------|------|
| 0x00 | HEARTBEAT | Both | 3 bytes |
| 0x01 | TIME | Host → ESP32 | 7-10 bytes |
| 0x02 | WEATHER | Host → ESP32 | 100-500 bytes |
| 0x03 | MEDIA | Host → ESP32 | 200-500 bytes |
| 0x04 | NOTIFICATION | Host → ESP32 | 100-500 bytes |
| 0x05 | PORTFOLIO | Host → ESP32 | 500-4000 bytes |
| 0x06 | SYSTEM | Host → ESP32 | 100-200 bytes |
| 0x07 | CALENDAR | Host → ESP32 | 200-2000 bytes |
| 0x08 | ACK | Both | 2 bytes |
| 0x09 | NACK | Both | 3 bytes |
| 0x0A | ERROR | Both | 10-100 bytes |
| 0x0B | CONFIG | Both | 50-256 bytes |
| 0x0C | REQUEST | ESP32 → Host | 2 bytes |
| 0x0D | TOUCH | ESP32 → Host | 9 bytes |
| 0x0E | COMMAND | Both | 10-256 bytes |
| 0x0F | DEBUG | Both | 10-1000 bytes |

### Error Code Summary

| Code | Name | Description |
|------|------|-------------|
| 0x00 | SUCCESS | No error |
| 0x01 | INVALID_FORMAT | Bad message format |
| 0x02 | CRC_ERROR | CRC mismatch |
| 0x03 | TIMEOUT | Operation timed out |
| 0x04 | BUFFER_OVERFLOW | Buffer full |
| 0x05 | NOT_IMPLEMENTED | Feature not available |
