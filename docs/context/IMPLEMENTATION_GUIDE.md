# Implementation Guide

## Overview

This guide provides step-by-step instructions for implementing the ESP32 Dashboard project. It covers both the **C++ Server Application** and the **ESP32 Firmware**, following the architecture and specifications defined in the other context files.

---

## Project Structure

### Repository Layout
```
LeDashboard/
├── docs/
│   └── context/                    # This directory
│       ├── PROJECT_OVERVIEW.md
│       ├── FEATURES.md
│       ├── TECHNICAL_SPECIFICATIONS.md
│       ├── COMMUNICATION_PROTOCOL.md
│       ├── API_INTEGRATIONS.md
│       ├── UI_DESIGN.md
│       └── IMPLEMENTATION_GUIDE.md
│
├── server/                       # C++ Server Application
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── config/
│   │   ├── config.hpp
│   │   └── settings.json
│   ├── core/
│   │   ├── server.hpp
│   │   └── serial_port.hpp
│   ├── data/
│   │   ├── data_source.hpp
│   │   ├── weather.hpp
│   │   ├── media.hpp
│   │   ├── notifications.hpp
│   │   ├── portfolio.hpp
│   │   └── system_monitor.hpp
│   ├── protocol/
│   │   ├── protocol.hpp
│   │   ├── encoder.hpp
│   │   └── decoder.hpp
│   └── utils/
│       ├── logger.hpp
│       └── helpers.hpp
│
├── firmware/                      # ESP32 Firmware
│   ├── platformio.ini
│   ├── src/
│   │   ├── main.cpp
│   │   ├── config/
│   │   │   ├── pins.hpp
│   │   │   ├── settings.hpp
│   │   │   └── display_config.hpp
│   │   ├── display/
│   │   │   ├── display.hpp
│   │   │   ├── touch.hpp
│   │   │   └── lvgl_helper.hpp
│   │   ├── ui/
│   │   │   ├── theme.hpp
│   │   │   ├── screens/
│   │   │   │   ├── home_screen.hpp
│   │   │   │   ├── weather_screen.hpp
│   │   │   │   └── ...
│   │   │   ├── widgets/
│   │   │   │   ├── weather_widget.hpp
│   │   │   │   └── ...
│   │   │   └── ui_manager.hpp
│   │   ├── serial/
│   │   │   ├── serial.hpp
│   │   │   ├── protocol.hpp
│   │   │   └── message_handler.hpp
│   │   ├── data/
│   │   │   └── data_store.hpp
│   │   └── tasks/
│   │       ├── display_task.hpp
│   │       ├── serial_task.hpp
│   │       └── touch_task.hpp
│   └── lib/
│       └── (LVGL, TFT_eSPI, etc.)
│
└── README.md
```

---

## Phase 1: Setup Development Environment

### Prerequisites

#### Windows (as configured on the dev machine — see DEV_ENVIRONMENT.md)
1. **Visual Studio 2022 Community** with "Desktop development with C++" workload (MSVC 14.40 + Windows 10 SDK)
2. **CMake 3.29.3** (`C:\Program Files\CMake\bin`) and **Ninja 1.13.2** (chocolatey)
3. **Git** - For version control
4. **vcpkg** at `C:\vcpkg`, bootstrapped; user env vars set: `VCPKG_ROOT=C:\vcpkg`, `C:\vcpkg` on `PATH`, `VCPKG_DEFAULT_TRIPLET=x64-windows`, `VCPKG_BINARY_SOURCES` (nuget binary cache)
5. **Anaconda** with the `le_dashboard_env` conda env — the only Python used in this project (pyserial); installs into it need an elevated prompt
6. **com0com** virtual null-modem (pair COM20 <-> COM21) for hardware-less serial testing — `server/tools/setup_com0com.ps1`, run elevated
7. **PlatformIO** - For ESP32 development
   - Install via VS Code extension or standalone
8. **USB Drivers** - For ESP32 board

#### Linux
1. **GCC/G++** - C++ compiler
2. **CMake** - Build system
3. **PlatformIO** - For ESP32 development
4. **Git** - Version control
5. **Python 3.8+** - Required by PlatformIO
6. **USB access** - Add user to dialout group

#### macOS
1. **Xcode Command Line Tools**
2. **Homebrew** - Package manager
3. **PlatformIO** - For ESP32 development
4. **Git** - Version control
5. **Python 3.8+**

---

### Install Dependencies

#### Server Application (C++)
```bash
# Clone the project
git clone <repository_url>
cd LeDashboard/server

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -G "Visual Studio 17 2022"  # Windows
cmake ..                          # Linux/macOS

# Build
cmake --build . --config Release
```

**Required Libraries** — declared in `server/vcpkg.json` and installed automatically
in vcpkg manifest mode when configuring with a preset (`cmake --preset windows-ninja`).
Classic mode (`vcpkg install <pkg>`) also works since `VCPKG_ROOT`/`PATH` are set:

- **cpr** (C++ Requests): HTTP requests
- **nlohmann/json**: JSON parsing
- **spdlog**: Logging
- **lz4**: Payload compression (enabled by default, see `USE_LZ4_COMPRESSION` in CMakeLists.txt)
- **doctest**: Unit tests
- **Boost.Asio**: Serial port (optional alternative to the native Windows API)

#### ESP32 Firmware (PlatformIO)
```bash
# Install PlatformIO
pio install

# Or via pip
pip install platformio

# Initialize project
cd LeDashboard/firmware
pio init -d esp32dev

# Install dependencies
pio lib install lovyan03/LovyanGFX@^1.1.5
pio lib install lvgl/lvgl@^8.3.0
pio lib install bblanchon/ArduinoJson@^6.20.0
```

---

## Phase 2: Implement Core Infrastructure

### Step 1: Communication Protocol

#### Server Side (C++)

**serial_port.hpp**:
```cpp
#pragma once
#include <string>
#include <vector>
#include <functional>

class SerialPort {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    
    SerialPort(const std::string& port_name, uint32_t baud_rate);
    ~SerialPort();
    
    bool open();
    void close();
    bool isOpen() const;
    
    size_t write(const std::vector<uint8_t>& data);
    void setDataCallback(DataCallback callback);
    void startReading();
    void stopReading();
    
private:
    std::string port_name_;
    uint32_t baud_rate_;
    bool open_;
    DataCallback data_callback_;
    
    // Platform-specific implementation
    #ifdef _WIN32
    void* handle_;
    #else
    int fd_;
    #endif
    
    void readThread();
};
```

**serial_port.cpp**:
```cpp
#include "serial_port.hpp"
#include <thread>
#include <atomic>

SerialPort::SerialPort(const std::string& port_name, uint32_t baud_rate)
    : port_name_(port_name), baud_rate_(baud_rate), open_(false) {}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open() {
    #ifdef _WIN32
    // Windows implementation using CreateFile
    handle_ = CreateFileA(port_name_.c_str(), GENERIC_READ | GENERIC_WRITE, 
                        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(handle_, &dcb);
    dcb.BaudRate = baud_rate_;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(handle_, &dcb);
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(handle_, &timeouts);
    
    #else
    // Linux/macOS implementation
    fd_ = ::open(port_name_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        return false;
    }
    
    struct termios options;
    tcgetattr(fd_, &options);
    cfsetispeed(&options, baud_rate_);
    cfsetospeed(&options, baud_rate_);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;
    tcsetattr(fd_, TCSANOW, &options);
    #endif
    
    open_ = true;
    return true;
}

void SerialPort::close() {
    if (!open_) return;
    
    stopReading();
    
    #ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    #else
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    #endif
    
    open_ = false;
}

size_t SerialPort::write(const std::vector<uint8_t>& data) {
    if (!open_) return 0;
    
    #ifdef _WIN32
    DWORD bytes_written = 0;
    WriteFile(handle_, data.data(), data.size(), &bytes_written, NULL);
    return bytes_written;
    #else
    return ::write(fd_, data.data(), data.size());
    #endif
}

void SerialPort::readThread() {
    std::vector<uint8_t> buffer(1024);
    while (open_) {
        #ifdef _WIN32
        DWORD bytes_read = 0;
        ReadFile(handle_, buffer.data(), buffer.size(), &bytes_read, NULL);
        #else
        ssize_t bytes_read = ::read(fd_, buffer.data(), buffer.size());
        #endif
        
        if (bytes_read > 0 && data_callback_) {
            data_callback_(std::vector<uint8_t>(buffer.begin(), buffer.begin() + bytes_read));
        }
    }
}
```

#### ESP32 Side

**serial.hpp**:
```cpp
#pragma once
#include <HardwareSerial.h>
#include <vector>
#include <functional>

class SerialManager {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    
    SerialManager(HardwareSerial& serial, uint32_t baud_rate = 115200);
    void begin();
    void setDataCallback(DataCallback callback);
    void send(const std::vector<uint8_t>& data);
    void process();
    
private:
    HardwareSerial& serial_;
    uint32_t baud_rate_;
    std::vector<uint8_t> receive_buffer_;
    DataCallback data_callback_;
};
```

**serial.cpp**:
```cpp
#include "serial.hpp"

SerialManager::SerialManager(HardwareSerial& serial, uint32_t baud_rate)
    : serial_(serial), baud_rate_(baud_rate) {
    receive_buffer_.reserve(4096);
}

void SerialManager::begin() {
    serial_.begin(baud_rate_);
}

void SerialManager::setDataCallback(DataCallback callback) {
    data_callback_ = callback;
}

void SerialManager::send(const std::vector<uint8_t>& data) {
    serial_.write(data.data(), data.size());
}

void SerialManager::process() {
    while (serial_.available()) {
        receive_buffer_.push_back(serial_.read());
    }
    
    if (!receive_buffer_.empty() && data_callback_) {
        data_callback_(receive_buffer_);
        receive_buffer_.clear();
    }
}
```

### Step 2: Protocol Implementation

#### Server Side (C++)

**protocol.hpp**:
```cpp
#pragma once
#include <vector>
#include <cstdint>

// Message types
enum class MessageType : uint8_t {
    HEARTBEAT = 0x00,
    TIME = 0x01,
    WEATHER = 0x02,
    MEDIA = 0x03,
    NOTIFICATION = 0x04,
    PORTFOLIO = 0x05,
    SYSTEM = 0x06,
    ACK = 0x08,
    NACK = 0x09,
    ERROR = 0x0A,
    CONFIG = 0x0B,
    REQUEST = 0x0C,
    TOUCH = 0x0D,
    COMMAND = 0x0E,
    DEBUG = 0x0F
};

// Frame constants
constexpr uint8_t SOF = 0xAA;
constexpr uint8_t EOF = 0x55;
constexpr uint8_t ESC = 0x5A;

class Protocol {
public:
    static std::vector<uint8_t> encodeMessage(MessageType type, const std::vector<uint8_t>& payload);
    static bool decodeMessage(const std::vector<uint8_t>& data, MessageType& type, std::vector<uint8_t>& payload);
    static uint16_t crc16(const uint8_t* buf, size_t len);
    
private:
    static void byteStuffing(std::vector<uint8_t>& data);
    static void byteUnstuffing(std::vector<uint8_t>& data);
};
```

**protocol.cpp**:
```cpp
#include "protocol.hpp"

uint16_t Protocol::crc16(const uint8_t *buf, size_t len) {
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

std::vector<uint8_t> Protocol::encodeMessage(MessageType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    
    // SOF
    frame.push_back(SOF);
    
    // Type
    frame.push_back(static_cast<uint8_t>(type));
    
    // Length (big-endian)
    uint16_t length = payload.size();
    frame.push_back(length >> 8);
    frame.push_back(length & 0xFF);
    
    // Calculate CRC
    uint16_t crc = crc16(&frame[1], 3); // Type + Length
    crc = crc16(payload.data(), payload.size(), crc);
    
    // Payload with byte stuffing
    std::vector<uint8_t> stuffed_payload = payload;
    byteStuffing(stuffed_payload);
    frame.insert(frame.end(), stuffed_payload.begin(), stuffed_payload.end());
    
    // CRC
    frame.push_back(crc >> 8);
    frame.push_back(crc & 0xFF);
    
    // EOF
    frame.push_back(EOF);
    
    return frame;
}

bool Protocol::decodeMessage(const std::vector<uint8_t>& data, MessageType& type, std::vector<uint8_t>& payload) {
    size_t pos = 0;
    
    // Find SOF
    while (pos < data.size() && data[pos] != SOF) pos++;
    if (pos >= data.size()) return false;
    pos++; // Skip SOF
    
    // Need at least: Type(1) + Length(2) + CRC(2) + EOF(1) = 6 bytes
    if (pos + 5 >= data.size()) return false;
    
    // Type
    type = static_cast<MessageType>(data[pos++]);
    
    // Length
    uint16_t length = (data[pos] << 8) | data[pos + 1];
    pos += 2;
    
    // Check if we have complete message
    size_t message_end = pos + length + 3; // payload + CRC + EOF
    if (message_end > data.size()) return false;
    
    // Extract payload
    std::vector<uint8_t> stuffed_payload(data.begin() + pos, data.begin() + pos + length);
    pos += length;
    
    // Byte unstuffing
    byteUnstuffing(stuffed_payload);
    payload = stuffed_payload;
    
    // CRC
    uint16_t received_crc = (data[pos] << 8) | data[pos + 1];
    pos += 2;
    
    // EOF
    if (data[pos] != EOF) return false;
    
    // Verify CRC
    uint16_t calculated_crc = crc16(&type, 1);
    calculated_crc = crc16(reinterpret_cast<const uint8_t*>(&length), 2, calculated_crc);
    calculated_crc = crc16(payload.data(), payload.size(), calculated_crc);
    
    return calculated_crc == received_crc;
}

void Protocol::byteStuffing(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ) {
        if (data[i] == SOF || data[i] == EOF || data[i] == ESC) {
            data.insert(data.begin() + i, ESC);
            data[i + 1] ^= 0xA5;
            i += 2;
        } else {
            i++;
        }
    }
}

void Protocol::byteUnstuffing(std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); ) {
        if (data[i] == ESC && i + 1 < data.size()) {
            data[i] = data[i + 1] ^ 0xA5;
            data.erase(data.begin() + i + 1);
        } else {
            i++;
        }
    }
}
```

#### ESP32 Side

Use the same protocol implementation (copy the header and adapt the CRC function for ESP32).

---

### Step 3: Data Source Framework

**data_source.hpp**:
```cpp
#pragma once
#include <string>
#include <cstdint>
#include <memory>

class DataSource {
public:
    virtual ~DataSource() = default;
    
    virtual bool initialize(const std::string& config) = 0;
    virtual void update() = 0;
    virtual bool isAvailable() const = 0;
    virtual std::string getLastError() const = 0;
    virtual uint64_t getLastUpdateTime() const = 0;
    virtual std::string getDataJson() const = 0;
    virtual std::vector<uint8_t> getDataBinary() const = 0;
    
    // Optional: For data that needs to be sent immediately
    virtual bool hasPendingUpdates() const { return false; }
};
```

---

## Phase 3: Implement Host Application (C++ Server)

### Step 1: Main Application

**main.cpp**:
```cpp
#include "server.hpp"
#include <iostream>

int main() {
    try {
        Server server("config.json");
        
        if (!server.initialize()) {
            std::cerr << "Failed to initialize server: " << server.getLastError() << std::endl;
            return 1;
        }
        
        std::cout << "Dashboard Server started. Press Ctrl+C to exit." << std::endl;
        
        server.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

**server.hpp**:
```cpp
#pragma once
#include "serial_port.hpp"
#include "protocol.hpp"
#include "data_source.hpp"
#include <vector>
#include <memory>
#include <string>

class Server {
public:
    Server(const std::string& config_file);
    ~Server();
    
    bool initialize();
    void run();
    void stop();
    std::string getLastError() const;
    
private:
    void loadConfiguration();
    void setupDataSources();
    void handleIncomingData(const std::vector<uint8_t>& data);
    void sendDataToEsp32();
    void sendHeartbeat();
    void processRequests();
    
    std::string config_file_;
    std::string last_error_;
    std::unique_ptr<SerialPort> serial_port_;
    std::vector<std::unique_ptr<DataSource>> data_sources_;
    
    // Configuration
    std::string serial_port_name_;
    uint32_t baud_rate_{115200};
    uint32_t update_interval_ms_{100};
    
    bool running_{false};
};
```

**server.cpp**:
```cpp
#include "server.hpp"
#include "weather.hpp"
#include "media.hpp"
#include "notifications.hpp"
#include "system_monitor.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Server::Server(const std::string& config_file)
    : config_file_(config_file) {}

Server::~Server() {
    stop();
}

bool Server::initialize() {
    try {
        loadConfiguration();
        
        // Open serial port
        serial_port_ = std::make_unique<SerialPort>(serial_port_name_, baud_rate_);
        if (!serial_port_->open()) {
            last_error_ = "Failed to open serial port: " + serial_port_name_;
            return false;
        }
        
        // Set up data sources
        setupDataSources();
        
        // Set up callback for incoming data
        serial_port_->setDataCallback([this](const std::vector<uint8_t>& data) {
            handleIncomingData(data);
        });
        
        running_ = true;
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

void Server::loadConfiguration() {
    std::ifstream file(config_file_);
    if (!file) {
        // Use defaults
        serial_port_name_ = "COM3"; // or /dev/ttyUSB0 on Linux
        baud_rate_ = 115200;
        return;
    }
    
    json config = json::parse(file);
    
    if (config.contains("server")) {
        auto server_config = config["server"];
        if (server_config.contains("port")) {
            serial_port_name_ = server_config["port"];
        }
        if (server_config.contains("baud_rate")) {
            baud_rate_ = server_config["baud_rate"];
        }
    }
}

void Server::setupDataSources() {
    // Create data sources based on configuration
    data_sources_.push_back(std::make_unique<WeatherDataSource>());
    data_sources_.push_back(std::make_unique<MediaDataSource>());
    data_sources_.push_back(std::make_unique<NotificationDataSource>());
    data_sources_.push_back(std::make_unique<SystemMonitorDataSource>());
    
    // Initialize all data sources
    for (auto& source : data_sources_) {
        source->initialize("");
    }
}

void Server::run() {
    // Start reading from serial port
    serial_port_->startReading();
    
    // Main loop
    while (running_) {
        // Update all data sources
        for (auto& source : data_sources_) {
            source->update();
        }
        
        // Send data to ESP32
        sendDataToEsp32();
        
        // Send periodic heartbeat
        sendHeartbeat();
        
        // Process requests from ESP32
        processRequests();
        
        // Sleep for update interval
        std::this_thread::sleep_for(std::chrono::milliseconds(update_interval_ms_));
    }
}

void Server::stop() {
    running_ = false;
    if (serial_port_) {
        serial_port_->stopReading();
        serial_port_->close();
    }
}

void Server::handleIncomingData(const std::vector<uint8_t>& data) {
    MessageType type;
    std::vector<uint8_t> payload;
    
    if (Protocol::decodeMessage(data, type, payload)) {
        switch (type) {
            case MessageType::HEARTBEAT:
                // Respond to heartbeat
                sendHeartbeat();
                break;
                
            case MessageType::ACK:
                // Acknowledgment received
                break;
                
            case MessageType::REQUEST:
                // Handle data request
                handleRequest(payload);
                break;
                
            case MessageType::TOUCH:
                // Handle touch event
                handleTouchEvent(payload);
                break;
                
            case MessageType::COMMAND:
                // Handle command
                handleCommand(payload);
                break;
                
            default:
                // Unknown message type
                break;
        }
    }
}

void Server::sendDataToEsp32() {
    // Send time
    auto time_data = getTimeData();
    if (!time_data.empty()) {
        auto message = Protocol::encodeMessage(MessageType::TIME, time_data);
        serial_port_->write(message);
    }
    
    // Send weather
    for (auto& source : data_sources_) {
        if (source->isAvailable() && source->hasPendingUpdates()) {
            auto json_data = source->getDataJson();
            std::vector<uint8_t> binary_data(json_data.begin(), json_data.end());
            
            MessageType type = getMessageTypeForSource(source.get());
            auto message = Protocol::encodeMessage(type, binary_data);
            serial_port_->write(message);
        }
    }
}

void Server::sendHeartbeat() {
    static uint32_t last_heartbeat = 0;
    uint32_t now = GetTickCount();
    
    if (now - last_heartbeat > 5000) { // 5 seconds
        std::vector<uint8_t> heartbeat_data = {0x00, 0x01, 0x00}; // Sequence number
        auto message = Protocol::encodeMessage(MessageType::HEARTBEAT, heartbeat_data);
        serial_port_->write(message);
        last_heartbeat = now;
    }
}

void Server::handleRequest(const std::vector<uint8_t>& payload) {
    if (!payload.empty()) {
        uint8_t request_type = payload[0];
        
        switch (request_type) {
            case 0x01: // Full sync
                sendFullSync();
                break;
            case 0x02: // Time sync
                sendTimeSync();
                break;
            // ... other request types
        }
    }
}

std::string Server::getLastError() const {
    return last_error_;
}
```

### Step 2: Implement Data Sources

#### Weather Data Source

**weather.hpp**:
```cpp
#pragma once
#include "data_source.hpp"
#include <string>

class WeatherDataSource : public DataSource {
public:
    WeatherDataSource();
    
    bool initialize(const std::string& config) override;
    void update() override;
    bool isAvailable() const override;
    std::string getLastError() const override;
    uint64_t getLastUpdateTime() const override;
    std::string getDataJson() const override;
    bool hasPendingUpdates() const override;
    
private:
    std::string api_key_;
    std::string location_;
    std::string units_;
    std::string last_error_;
    uint64_t last_update_{0};
    std::string last_data_;
    bool pending_updates_{false};
};
```

**weather.cpp**:
```cpp
#include "weather.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WeatherDataSource::WeatherDataSource() {}

bool WeatherDataSource::initialize(const std::string& config) {
    // Parse config
    try {
        json config_json = json::parse(config);
        if (config_json.contains("api_key")) {
            api_key_ = config_json["api_key"];
        }
        if (config_json.contains("location")) {
            location_ = config_json["location"];
        }
        if (config_json.contains("units")) {
            units_ = config_json["units"];
        }
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return false;
    }
}

void WeatherDataSource::update() {
    try {
        std::string url = "https://api.openweathermap.org/data/2.5/weather";
        url += "?q=" + cpr::util::urlEncode(location_);
        url += "&appid=" + api_key_;
        url += "&units=" + units_;
        
        cpr::Response r = cpr::Get(cpr::Url{url});
        
        if (r.status_code == 200) {
            json data = json::parse(r.text);
            last_data_ = processWeatherData(data);
            last_update_ = std::time(nullptr);
            pending_updates_ = true;
        } else {
            last_error_ = "HTTP " + std::to_string(r.status_code);
        }
    } catch (const std::exception& e) {
        last_error_ = e.what();
    }
}

std::string WeatherDataSource::processWeatherData(const json& api_data) {
    json result;
    
    result["timestamp"] = api_data["dt"];
    result["location"] = api_data["name"].get<std::string>() + ", " + 
                        api_data["sys"]["country"].get<std::string>();
    result["current"]["temperature"] = api_data["main"]["temp"].get<double>();
    result["current"]["feels_like"] = api_data["main"]["feels_like"].get<double>();
    result["current"]["humidity"] = api_data["main"]["humidity"].get<int>();
    result["current"]["pressure"] = api_data["main"]["pressure"].get<int>();
    result["current"]["wind_speed"] = api_data["wind"]["speed"].get<double>();
    result["current"]["wind_direction"] = api_data["wind"]["deg"].get<int>();
    result["current"]["condition"] = api_data["weather"][0]["main"].get<std::string>();
    result["current"]["condition_code"] = api_data["weather"][0]["icon"].get<std::string>();
    result["current"]["sunrise"] = api_data["sys"]["sunrise"].get<int>();
    result["current"]["sunset"] = api_data["sys"]["sunset"].get<int>();
    
    return result.dump();
}

// Other methods...
```

---

## Phase 4: Implement ESP32 Firmware

### Step 1: Main Application

**platformio.ini**:
```ini
[env:esp32-4848s040]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

board_build.partitions = default

lib_deps =
    lovyan03/LovyanGFX@^1.1.5
    lvgl/lvgl@^8.3.0
    lvgl/lvgl_esp32_drivers@^1.0.0
    bblanchon/ArduinoJson@^6.20.0

build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DLVGL_USE_DISP_DRAW_BUF
    -DLVGL_DRAW_BUF_SIZE=8192
```

**main.cpp**:
```cpp
#include "display/display.hpp"
#include "serial/serial.hpp"
#include "ui/ui_manager.hpp"
#include "data/data_store.hpp"

Display display;
SerialManager serial(Serial, 115200);
DataStore dataStore;
UIManager uiManager(display, dataStore);

void setup() {
    // Initialize serial
    serial.begin();
    
    // Initialize display
    display.begin();
    
    // Set up serial callback
    serial.setDataCallback([&](const std::vector<uint8_t>& data) {
        uiManager.handleSerialData(data);
    });
    
    // Initialize UI
    uiManager.begin();
}

void loop() {
    // Process serial data
    serial.process();
    
    // Update UI
    uiManager.update();
    
    // Render display
    display.update();
}
```

### Step 2: Display Initialization

**display/display.hpp**:
```cpp
#pragma once
#include <lvgl.h>

class Display {
public:
    void begin();
    void update();
    lv_disp_t* getDisplay() const;
    
private:
    void initializeLVGL();
    void initializeDisplayDriver();
    void initializeTouchDriver();
    
    lv_disp_t* disp_{nullptr};
    lv_indev_t* indev_{nullptr};
};
```

**display/display.cpp**:
```cpp
#include "display.hpp"
#include "lvgl_esp32_drivers/esp32_driver.h"

void Display::begin() {
    initializeLVGL();
    initializeDisplayDriver();
    initializeTouchDriver();
}

void Display::initializeLVGL() {
    lv_init();
    
    // Allocate display buffer
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[480 * 60];
    static lv_color_t buf2[480 * 60];
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 480 * 60);
    
    // Initialize display
    disp_ = lv_disp_create(480, 480);
    lv_disp_set_draw_buf(disp_, &draw_buf);
    lv_disp_set_flush_cb(disp_, display_flush);
}

void Display::initializeDisplayDriver() {
    // Use LVGL ESP32 driver
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 480;
    disp_drv.flush_cb = display_flush;
    lv_disp_drv_register(&disp_drv);
}

void Display::initializeTouchDriver() {
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    indev_ = lv_indev_drv_register(&indev_drv);
}

void Display::update() {
    lv_timer_handler();
}

lv_disp_t* Display::getDisplay() const {
    return disp_;
}

// Callbacks
void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    // Implementation depends on display driver
    // For TFT_eSPI:
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);
    tft.writePixels(color_map, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1));
    tft.endWrite();
    
    lv_disp_flush_ready(drv);
}

bool touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    // Implementation depends on touch driver
    // For XPT2046:
    if (touchAvailable()) {
        TS_Point p = getTouchPoint();
        if (p.z > TOUCH_THRESHOLD) {
            data->point.x = p.x;
            data->point.y = p.y;
            data->state = LV_INDEV_STATE_PRESSED;
            return true;
        }
    }
    data->state = LV_INDEV_STATE_RELEASED;
    return false;
}
```

### Step 3: UI Manager

**ui/ui_manager.hpp**:
```cpp
#pragma once
#include "display/display.hpp"
#include "data/data_store.hpp"
#include "ui/screens/home_screen.hpp"

class UIManager {
public:
    UIManager(Display& display, DataStore& dataStore);
    
    void begin();
    void update();
    void handleSerialData(const std::vector<uint8_t>& data);
    
private:
    void createScreens();
    void handleMessage(MessageType type, const std::vector<uint8_t>& payload);
    
    Display& display_;
    DataStore& dataStore_;
    
    HomeScreen* homeScreen_{nullptr};
    WeatherScreen* weatherScreen_{nullptr};
    MediaScreen* mediaScreen_{nullptr};
    // ... other screens
    
    lv_obj_t* currentScreen_{nullptr};
};
```

**ui/ui_manager.cpp**:
```cpp
#include "ui_manager.hpp"
#include "protocol.hpp"

UIManager::UIManager(Display& display, DataStore& dataStore)
    : display_(display), dataStore_(dataStore) {}

void UIManager::begin() {
    createScreens();
    
    // Show home screen
    lv_obj_clear_flag(homeScreen_->getObject(), LV_OBJ_FLAG_HIDDEN);
    currentScreen_ = homeScreen_->getObject();
}

void UIManager::createScreens() {
    lv_obj_t* scr = lv_obj_create(lv_scr_act());
    homeScreen_ = new HomeScreen(scr, dataStore_);
    
    scr = lv_obj_create(lv_scr_act());
    weatherScreen_ = new WeatherScreen(scr, dataStore_);
    
    scr = lv_obj_create(lv_scr_act());
    mediaScreen_ = new MediaScreen(scr, dataStore_);
    
    // Hide all except home
    lv_obj_add_flag(weatherScreen_->getObject(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mediaScreen_->getObject(), LV_OBJ_FLAG_HIDDEN);
}

void UIManager::update() {
    // Update all screens
    homeScreen_->update();
    weatherScreen_->update();
    mediaScreen_->update();
}

void UIManager::handleSerialData(const std::vector<uint8_t>& data) {
    MessageType type;
    std::vector<uint8_t> payload;
    
    if (Protocol::decodeMessage(data, type, payload)) {
        handleMessage(type, payload);
    }
}

void UIManager::handleMessage(MessageType type, const std::vector<uint8_t>& payload) {
    switch (type) {
        case MessageType::TIME:
            dataStore_.updateTime(payload);
            homeScreen_->updateTime();
            break;
        case MessageType::WEATHER:
            dataStore_.updateWeather(payload);
            homeScreen_->updateWeather();
            weatherScreen_->updateWeather();
            break;
        case MessageType::MEDIA:
            dataStore_.updateMedia(payload);
            homeScreen_->updateMedia();
            mediaScreen_->updateMedia();
            break;
        case MessageType::NOTIFICATION:
            dataStore_.updateNotifications(payload);
            homeScreen_->updateNotifications();
            break;
        case MessageType::PORTFOLIO:
            dataStore_.updatePortfolio(payload);
            break;
        case MessageType::SYSTEM:
            dataStore_.updateSystem(payload);
            break;
        case MessageType::REQUEST:
            // Handle request from ESP32
            break;
        case MessageType::TOUCH:
            // Forward touch to current screen
            handleTouchEvent(payload);
            break;
        default:
            break;
    }
}
```

### Step 4: Home Screen Implementation

**ui/screens/home_screen.hpp**:
```cpp
#pragma once
#include "data/data_store.hpp"

class HomeScreen {
public:
    HomeScreen(lv_obj_t* parent, DataStore& dataStore);
    
    void update();
    void updateTime();
    void updateWeather();
    void updateMedia();
    void updateNotifications();
    
    lv_obj_t* getObject() const;
    
private:
    void createUI();
    
    lv_obj_t* parent_{nullptr};
    DataStore& dataStore_;
    
    // Widgets
    lv_obj_t* timeWidget_{nullptr};
    lv_obj_t* weatherWidget_{nullptr};
    lv_obj_t* mediaWidget_{nullptr};
    lv_obj_t* notificationWidget_{nullptr};
    lv_obj_t* systemWidget_{nullptr};
    lv_obj_t* navBar_{nullptr};
};
```

**ui/screens/home_screen.cpp**:
```cpp
#include "home_screen.hpp"
#include "widgets/time_widget.hpp"
#include "widgets/weather_widget.hpp"
#include "widgets/media_widget.hpp"

HomeScreen::HomeScreen(lv_obj_t* parent, DataStore& dataStore)
    : parent_(parent), dataStore_(dataStore) {
    createUI();
}

void HomeScreen::createUI() {
    lv_obj_set_layout(parent_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent_, 0, 0);
    lv_obj_set_style_pad_gap(parent_, 0, 0);
    
    // Status bar
    // ...
    
    // Main content grid
    lv_obj_t* grid = lv_obj_create(parent_);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    lv_obj_set_style_pad_all(grid, 8, 0);
    lv_obj_set_flex_grow(grid, 1);
    
    // Time widget
    lv_obj_t* timeCont = lv_obj_create(grid);
    lv_obj_set_grid_cell(timeCont, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(timeCont, LV_GRID_ALIGN_STRETCH, 0, 1);
    timeWidget_ = new TimeWidget(timeCont, dataStore_);
    
    // Weather widget
    lv_obj_t* weatherCont = lv_obj_create(grid);
    lv_obj_set_grid_cell(weatherCont, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_grid_cell(weatherCont, LV_GRID_ALIGN_STRETCH, 0, 1);
    weatherWidget_ = new WeatherWidget(weatherCont, dataStore_);
    
    // Media widget
    lv_obj_t* mediaCont = lv_obj_create(grid);
    lv_obj_set_grid_cell(mediaCont, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_grid_cell(mediaCont, LV_GRID_ALIGN_STRETCH, 1, 1);
    mediaWidget_ = new MediaWidget(mediaCont, dataStore_);
    
    // Portfolio widget
    lv_obj_t* portfolioCont = lv_obj_create(grid);
    lv_obj_set_grid_cell(portfolioCont, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_grid_cell(portfolioCont, LV_GRID_ALIGN_STRETCH, 1, 1);
    // ...
    
    // Notification widget
    // ...
    
    // System widget
    // ...
    
    // Navigation bar
    navBar_ = createNavBar(parent_, dataStore_);
    lv_obj_set_pos(navBar_, 0, 480 - 60);
    lv_obj_set_size(navBar_, 480, 60);
}

void HomeScreen::update() {
    updateTime();
    updateWeather();
    updateMedia();
    updateNotifications();
}

void HomeScreen::updateTime() {
    if (timeWidget_) {
        timeWidget_->update();
    }
}

// ... other update methods

lv_obj_t* HomeScreen::getObject() const {
    return parent_;
}
```

---

## Phase 5: Implement Widgets

### Time Widget

**ui/widgets/time_widget.hpp**:
```cpp
#pragma once
#include "data/data_store.hpp"

class TimeWidget {
public:
    TimeWidget(lv_obj_t* parent, DataStore& dataStore);
    void update();
    
private:
    lv_obj_t* parent_{nullptr};
    DataStore& dataStore_;
    
    lv_obj_t* timeLabel_{nullptr};
    lv_obj_t* dateLabel_{nullptr};
    lv_obj_t* dayLabel_{nullptr};
};
```

**ui/widgets/time_widget.cpp**:
```cpp
#include "time_widget.hpp"
#include <ctime>

TimeWidget::TimeWidget(lv_obj_t* parent, DataStore& dataStore)
    : parent_(parent), dataStore_(dataStore) {
    
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_gap(parent, 4, 0);
    
    // Time label
    timeLabel_ = lv_label_create(parent);
    lv_obj_set_style_text_font(timeLabel_, &lv_font_montserrat_64, 0);
    lv_label_set_text(timeLabel_, "12:34");
    
    // Date label
    dateLabel_ = lv_label_create(parent);
    lv_obj_set_style_text_font(dateLabel_, &lv_font_montserrat_20, 0);
    lv_label_set_text(dateLabel_, "October 1");
    
    // Day label
    dayLabel_ = lv_label_create(parent);
    lv_obj_set_style_text_font(dayLabel_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dayLabel_, lv_color_hex(0x616161), 0);
    lv_label_set_text(dayLabel_, "Monday");
}

void TimeWidget::update() {
    auto timeData = dataStore_.getTimeData();
    
    if (timeData) {
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                timeData->hour, timeData->minute, timeData->second);
        lv_label_set_text(timeLabel_, timeStr);
        
        // Format date
        char dateStr[32];
        snprintf(dateStr, sizeof(dateStr), "%s %d", 
                getMonthName(timeData->month), timeData->day);
        lv_label_set_text(dateLabel_, dateStr);
        
        // Day of week
        lv_label_set_text(dayLabel_, getDayName(timeData->day_of_week));
    }
}
```

---

## Phase 6: Testing and Debugging

### Testing Strategy

#### Unit Tests (Server)
```cpp
// tests/test_protocol.cpp
#include <catch2/catch.hpp>
#include "protocol.hpp"

TEST_CASE("Protocol encoding/decoding", "[protocol]") {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    auto encoded = Protocol::encodeMessage(MessageType::TIME, payload);
    
    MessageType type;
    std::vector<uint8_t> decoded;
    REQUIRE(Protocol::decodeMessage(encoded, type, decoded));
    REQUIRE(type == MessageType::TIME);
    REQUIRE(decoded == payload);
}

TEST_CASE("Protocol with special bytes", "[protocol]") {
    std::vector<uint8_t> payload = {0xAA, 0x55, 0x5A, 0x01, 0x02};
    auto encoded = Protocol::encodeMessage(MessageType::WEATHER, payload);
    
    MessageType type;
    std::vector<uint8_t> decoded;
    REQUIRE(Protocol::decodeMessage(encoded, type, decoded));
    REQUIRE(type == MessageType::WEATHER);
    REQUIRE(decoded == payload);
}
```

#### Integration Tests
```cpp
// tests/integration_test.cpp
TEST_CASE("Server-ESP32 communication", "[integration]") {
    // Start server
    Server server("test_config.json");
    REQUIRE(server.initialize());
    
    // Start server in thread
    std::thread serverThread([&] { server.run(); });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Simulate ESP32 sending heartbeat
    // ...
    
    // Verify response
    // ...
    
    server.stop();
    serverThread.join();
}
```

### Debugging Tools

#### Server Debugging
```cpp
// Use spdlog for logging
#include <spdlog/spdlog.h>

int main() {
    auto console = spdlog::stdout_color_mt("console");
    console->set_level(spdlog::level::debug);
    console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    console->info("Server starting...");
    // ...
}
```

#### ESP32 Debugging
```cpp
// Use Serial for debugging
#define DEBUG_LEVEL 3 // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug

#if DEBUG_LEVEL >= 3
#define LOG_INFO(tag, ...) Serial.printf("[INFO] [%s] ", tag); Serial.printf(__VA_ARGS__); Serial.println();
#else
#define LOG_INFO(tag, ...)
#endif

void setup() {
    Serial.begin(115200);
    LOG_INFO("MAIN", "Starting setup");
    // ...
}
```

---

## Phase 7: Deployment

### Server Deployment (Windows)

#### As a Windows Service
1. Create a Windows service wrapper
2. Install using `sc create` command
3. Start service: `net start DashboardServer`

#### As a Startup Application
1. Create a shortcut to the executable
2. Place in `shell:startup` folder
3. Or add to registry: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`

### Server Deployment (Linux)

#### As a systemd Service
1. Create `/etc/systemd/system/dashboard-server.service`:
```ini
[Unit]
Description=Dashboard Server
After=network.target

[Service]
Type=simple
User=your_username
WorkingDirectory=/path/to/LeDashboard/server
ExecStart=/path/to/dashboard_server
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
2. Enable and start: `sudo systemctl enable dashboard-server && sudo systemctl start dashboard-server`

### ESP32 Deployment

#### Via PlatformIO
```bash
# Build and upload
pio run -t upload

# Or build and upload separately
pio run -t build
pio run -t upload --target upload_protocol
```

#### Via ESP-IDF
```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

#### OTA Update
1. Configure OTA partition
2. Create OTA update server
3. Send firmware updates wirelessly

---

## Troubleshooting

### Common Issues

#### Serial Communication Not Working
1. **Check port name**: Verify correct COM port / tty device
2. **Check baud rate**: Ensure both sides use same rate
3. **Check wiring**: Verify TX-RX crossover
4. **Check ground**: Ensure common ground connection
5. **Check drivers**: Install USB drivers for ESP32
6. **Test with loopback**: Connect TX to RX, send data and verify it's received

#### Display Not Working
1. **Check wiring**: Verify all display connections
2. **Check power**: Ensure adequate power supply
3. **Check driver configuration**: Verify TFT_eSPI settings
4. **Test with example**: Run LVGL example to verify display
5. **Check backlight**: Some displays need backlight enabled

#### Touch Not Working
1. **Check wiring**: Verify touch controller connections
2. **Check calibration**: Run touch calibration
3. **Check sensitivity**: Adjust touch threshold
4. **Test with example**: Run touch example
5. **Check controller**: Verify correct touch controller driver

#### Memory Issues
1. **Check heap**: Use `esp_get_free_heap_size()`
2. **Check fragmentation**: Use heap tracing
3. **Reduce usage**: Optimize memory usage
4. **Increase stack**: Increase task stack sizes
5. **Use PSRAM**: Enable PSRAM if available

#### Performance Issues
1. **Reduce update rate**: Limit data updates
2. **Simplify UI**: Reduce widget complexity
3. **Optimize rendering**: Use LVGL optimization flags
4. **Use double buffering**: Enable in LVGL config
5. **Profile code**: Identify bottlenecks

---

## Configuration

### Server Configuration (config.json)
```json
{
  "server": {
    "port": "COM3",
    "baud_rate": 115200,
    "log_level": "info",
    "log_file": "dashboard_server.log"
  },
  "integrations": {
    "weather": {
      "enabled": true,
      "provider": "openweathermap",
      "api_key": "your_api_key",
      "location": "Paris,FR",
      "units": "metric",
      "update_interval": 600
    },
    "thunderbird": {
      "enabled": true,
      "mail_directory": "C:\\Users\\user\\AppData\\Roaming\\Thunderbird\\Profiles\\xxxx.default\\Mail",
      "update_interval": 10
    },
    "media_player": {
      "enabled": true,
      "preferred_player": "spotify",
      "update_interval": 1
    },
    "system_monitor": {
      "enabled": true,
      "update_interval": 2
    }
  }
}
```

### ESP32 Configuration

**pins.hpp**:
```cpp
// Display pins (ESP32-4848S040)
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

// Touch pins
#define TOUCH_CS  27
#define TOUCH_IRQ 34
```

**settings.hpp**:
```cpp
// Display settings
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 480
#define DISPLAY_COLOR_DEPTH 16

// Serial settings
#define SERIAL_BAUD_RATE 115200
#define SERIAL_BUFFER_SIZE 4096

// UI settings
#define UI_UPDATE_INTERVAL 16 // ms (~60 FPS)
#define ANIMATION_DURATION 200 // ms
```

---

## Next Steps

After completing this implementation guide, you should have:

1. ✅ C++ Server Application running on your computer
2. ✅ ESP32 Firmware with LVGL UI
3. ✅ Working serial communication between them
4. ✅ Basic data display (time, weather, etc.)

Next phases:
- **Phase 8**: Add more features (portfolio, notifications, etc.)
- **Phase 9**: Optimize performance and memory usage
- **Phase 10**: Add customization options
- **Phase 11**: Test and refine

See the feature specifications for detailed feature implementations.
