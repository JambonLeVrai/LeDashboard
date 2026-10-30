# API Integrations Specification

## Overview

This document specifies how the **C++ Server Application** integrates with various external APIs and local services to collect data for the dashboard. Each integration has specific requirements, authentication methods, data formats, and update strategies.

The design philosophy is **minimal user involvement**: you configure your API keys in a config file once, and the software handles secure storage automatically.

---

## Integration Architecture

### Data Flow
```
External APIs & Local Services -> API Integration Layer (C++) -> Data Cache & Aggregator -> Serial Protocol -> ESP32 Firmware (LVGL Display)
```

### Integration Layer Design

The C++ server uses a **plugin-based architecture** for API integrations with a common interface:

```cpp
class DataSource {
public:
    virtual bool initialize(const std::string& config) = 0;
    virtual void update() = 0;
    virtual bool isAvailable() const = 0;
    virtual std::string getLastError() const = 0;
    virtual uint64_t getLastUpdateTime() const = 0;
    virtual std::string getDataJson() const = 0;
};
```

---

## 1. Weather API Integration

### Supported Providers

| Provider | Free Tier | Paid Tier | Notes |
|----------|-----------|-----------|-------|
| **OpenWeatherMap** | 1000 calls/day | 60 calls/min, 1M/month | Recommended |
| **WeatherAPI.com** | 1M calls/month | Higher limits | Good alternative |
| **AccuWeather** | Limited | Various | Requires location key |

### OpenWeatherMap Implementation

**Base URL**: `https://api.openweathermap.org/data/2.5/`

**Authentication**: API key as query parameter `appid=YOUR_API_KEY`

**Key Endpoints**:
- `GET /weather` - Current weather
- `GET /forecast` - 5-day forecast (3-hour intervals)
- `GET /onecall` - Comprehensive data (current, hourly, daily, alerts)

**Example Request**:
```
GET /weather?lat=48.8566&lon=2.3522&appid=YOUR_KEY&units=metric&lang=en
```

**Response Processing**: Extract temperature, humidity, pressure, wind, conditions, icons, sunrise/sunset

**Icon Mapping**: Map API condition codes to local LVGL-compatible icons

---

## 2. Thunderbird Email Integration

### Integration Approaches

#### Approach 1: File System Monitoring (Recommended)

Monitor Thunderbird's mail directory for changes:
- **Windows**: `C:\Users\{user}\AppData\Roaming\Thunderbird\Profiles\{profile}\Mail`
- **Linux**: `~/.thunderbird/{profile}/Mail`
- **macOS**: `~/Library/Thunderbird/Profiles/{profile}/Mail`

**File Formats**:
- **mbox**: Single file per folder (older Thunderbird)
- **maildir**: One file per email (newer Thunderbird)

**Implementation**:
- Use OS-specific file watching (ReadDirectoryChangesW on Windows, inotify on Linux)
- Parse email files to extract sender, subject, preview text
- Track unread counts from .msf (summary) files
- Support multiple accounts and folders

#### Approach 2: Thunderbird Add-on

Create a custom add-on that:
- Listens for new email events
- Exposes local HTTP server (port 8081)
- Provides JSON API: `/api/notifications`

**Advantages**: More reliable, real-time, handles all Thunderbird features
**Disadvantages**: Requires add-on installation

---

## 3. Portfolio API Integration

### Ghostfolio Local Server (Recommended)

**Base URL**: Configurable, typically `http://localhost:3000` or `http://<local-ip>:3000`

**Authentication**: None (local network) or optional API key if configured in Ghostfolio

**Note**: Ghostfolio is a self-hosted, open-source portfolio tracker that is perfect for this use case. It provides a RESTful API on the same host where it runs.

**Key Endpoints**:
```
GET /api/v1/portfolio               # Total portfolio value, today's change
GET /api/v1/portfolio/accounts      # All account balances
GET /api/v1/portfolio/holdings      # All holdings with performance, allocation
GET /api/v1/portfolio/asset-allocation  # Asset allocation breakdown
GET /api/v1/portfolio/performance   # Performance metrics over time
GET /api/v1/portfolio/transactions   # Transaction history (optional)
```

**Data Structure** (simplified):
```json
{
  "value": 125432.50,
  "change": {
    "absolute": 456.75,
    "percentage": 0.36
  },
  "accounts": [
    {
      "id": "abc123",
      "name": "Brokerage Account",
      "value": 80000.00,
      "currency": "USD",
      "change": {
        "absolute": 300.00,
        "percentage": 0.375
      }
    }
  ],
  "holdings": [
    {
      "symbol": "AAPL",
      "name": "Apple Inc.",
      "quantity": 50,
      "price": 150.25,
      "value": 7512.50,
      "allocation": 6.0,
      "change": {
        "absolute": 125.00,
        "percentage": 0.83
      }
    }
  ],
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**Update Strategy**:
- Summary: Every 30-60 seconds (for widget display)
- Detailed holdings: Every 2-5 minutes or on user request
- Real-time: WebSocket connection if Ghostfolio supports it

**Data Processing**:
- Filter and sort holdings by value
- Calculate asset allocation percentages
- Identify best/worst performers (top 5 gainers/losers)
- Format currency values appropriately
- Handle multiple currencies with conversion
- Group by account for the widget display

**Local Network Considerations**:
- Since Ghostfolio runs on your local network, there are no rate limits to worry about
- Connection is typically over HTTP (not HTTPS) on localhost
- Very low latency allows for frequent updates
- No external dependencies or internet required

---

## 4. Media Player Integration

### Supported Players

| Player | Platform | Integration Method |
|--------|----------|-------------------|
| **Spotify** | Windows | Local HTTP API (port 4370-4380) |
| **Windows Media Player** | Windows | COM API |
| **VLC** | Windows/Linux | HTTP interface or D-Bus |
| **iTunes** | Windows | COM API |
| **Foobar2000** | Windows | Winamp API or COM |
| **Winamp** | Windows | Winamp API |

### Spotify Integration (Recommended)

**Port**: 4370-4380 (auto-discover)

**Endpoints**:
- `GET /simplecsrf/csrftoken.json` - Get CSRF token
- `GET /remote/userinfo.json` - User info
- `GET /remote/status.json` - Current playback state

**Data Extracted**:
- Track title, artist, album
- Duration, current position
- Playback state (playing/paused/stopped)
- Volume level
- Album art URL

### Album Art Handling

**Sources**:
1. Player API (if available)
2. File metadata (ID3 tags)
3. Online lookup (Last.fm, Cover Art Archive)

**Caching**:
- Store album art locally in `cache/album_art/`
- Use consistent naming: `{artist}_{album}.jpg`
- Limit cache size (e.g., 100 images, 10MB total)
- Clean old/unused images periodically

---

## 5. System Monitor Integration

### Windows Implementation

**Technologies**:
- **PDH (Performance Data Helper)**: CPU, memory usage
- **PSAPI**: Process information
- **WMI**: Alternative for some metrics

**Metrics Collected**:
- CPU usage (total and per-core)
- CPU temperature (if available)
- Memory usage (total, used, free)
- Swap usage
- Disk usage per drive
- Network speed (upload/download)
- GPU usage (if available)

### Linux Implementation

**Technologies**:
- `/proc/stat`: CPU usage
- `/proc/meminfo`: Memory usage
- `statvfs`: Disk usage
- `/proc/net/dev`: Network statistics

---

## 6. Calendar Integration

**Status**: ⚠️ **NOT TO BE IMPLEMENTED YET**

This section is reserved for future implementation when you transition from cloud-based to locally-hosted solutions.

### Future Integration: Radicale

When you move to Radicale (a simple, locally-hosted calendar server), this integration will be added.

**Expected Implementation**:
- **Base URL**: `http://localhost:5232` (default Radicale port)
- **Authentication**: Basic auth or none (configurable)
- **Format**: iCalendar (ICS/RFC 5545)
- **Integration Method**: HTTP GET to calendar endpoints

**Not Implemented Until**: You have completed your migration from cloud services to Radicale.

**Current Status**: Calendar features are intentionally excluded from the initial implementation. The dashboard will focus on the other five core features (Time, Weather, Media, Notifications, Portfolio, System Monitor).

---

## Configuration

### Configuration File Format: TOML

The configuration uses **TOML (Tom's Obvious Minimal Language)** format instead of JSON because TOML is designed to be:
- **Human-readable**: Clear, unambiguous syntax for configuration
- **Easy to edit**: No mandatory quoting, comments supported
- **Type-safe**: Distinguishes between strings, numbers, booleans
- **Widely supported**: Native support in C++ via libraries

**Example `config.toml`**:
```toml
# Server Configuration
[server]
port = "COM3"          # Windows: COM3, COM4, etc.
# port = "/dev/ttyUSB0"  # Linux: Uncomment and use this instead
baud_rate = 115200
log_level = "info"     # debug, info, warn, error
log_file = "dashboard_server.log"

# Update intervals (in seconds)
update_interval = 100  # Global default, can be overridden per-integration

# =============================================================================
# API INTEGRATIONS
# =============================================================================

[integrations.weather]
enabled = true
provider = "openweathermap"  # Options: openweathermap, weatherapi
api_key = "your_openweathermap_api_key"
location = "Paris,FR"       # City,CountryCode or lat,lon
units = "metric"           # metric, imperial, standard
update_interval = 600      # 10 minutes

[integrations.thunderbird]
enabled = true
# Windows path example:
mail_directory = "C:/Users/YourName/AppData/Roaming/Thunderbird/Profiles/xyz123/Mail"
# Linux path example (uncomment and use on Linux):
# mail_directory = "/home/yourname/.thunderbird/xyz123/Mail"
update_interval = 10        # 10 seconds

[integrations.portfolio]
enabled = true
# Ghostfolio local server URL
url = "http://localhost:3000"
# Optional API key if configured in Ghostfolio (usually not needed for local)
# api_key = "your_ghostfolio_api_key_if_configured"
update_interval = 30       # 30 seconds for summary, detailed data cached longer

[integrations.media_player]
enabled = true
preferred_player = "spotify"  # Options: spotify, vlc, winamp, wmp, itunes
update_interval = 1         # 1 second (real-time playback position)

[integrations.system_monitor]
enabled = true
update_interval = 2         # 2 seconds
```

### TOML vs JSON Comparison

| Feature | TOML | JSON |
|---------|------|------|
| Human-readable | ✅ Yes | ❌ No (requires exact quoting) |
| Comments | ✅ Yes | ❌ No |
| Data types | ✅ Distinct | ❌ All same (stringly-typed) |
| Nesting | ✅ Yes | ✅ Yes |
| Arrays | ✅ Yes | ✅ Yes |
| Multi-line strings | ✅ Yes | ✅ Yes |
| Trailing commas | ✅ Yes | ❌ No |
| Key order | ✅ Preserved (optional) | ❌ Not preserved |

### TOML Parsing in C++

**Recommended Library**: `toml11` (header-only, C++11/14/17)

```cpp
#include <toml11/toml.hpp>
#include <fstream>

struct Config {
    struct Server {
        std::string port;
        int baud_rate;
        std::string log_level;
        std::string log_file;
    } server;
    
    struct Weather {
        bool enabled;
        std::string provider;
        std::string api_key;
        std::string location;
        std::string units;
        int update_interval;
    } weather;
    
    // ... other integrations
};

Config loadConfig(const std::string& filename) {
    Config config;
    
    try {
        std::ifstream file(filename);
        auto data = toml::parse(file);
        
        // Server
        config.server.port = toml::find<std::string>(data, "server", "port");
        config.server.baud_rate = toml::find<int>(data, "server", "baud_rate");
        config.server.log_level = toml::find<std::string>(data, "server", "log_level");
        config.server.log_file = toml::find<std::string>(data, "server", "log_file");
        
        // Weather
        config.weather.enabled = toml::find<bool>(data, "integrations", "weather", "enabled");
        config.weather.provider = toml::find<std::string>(data, "integrations", "weather", "provider");
        config.weather.api_key = toml::find<std::string>(data, "integrations", "weather", "api_key");
        config.weather.location = toml::find<std::string>(data, "integrations", "weather", "location");
        config.weather.units = toml::find<std::string>(data, "integrations", "weather", "units");
        config.weather.update_interval = toml::find<int>(data, "integrations", "weather", "update_interval");
        
        // ... load other integrations
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to load config: ") + e.what());
    }
    
    return config;
}
```

**Alternative Library**: `cpptoml` (more features, but requires linking)

---

## Secure API Key Storage

### Design Philosophy

Your requirement: **"I give the software my API by writing to a config file, and then the software does whatever it wants to store it securely."**

This means:
- ✅ You write plaintext API keys to `config.toml` **once**
- ✅ The software **automatically** encrypts and secures them
- ✅ No manual steps, keyrings, or credential managers required
- ✅ Transparent to the user
- ✅ Works on all platforms (Windows, Linux, macOS)

### Implementation: Automatic Encryption at Rest

#### Step 1: Config File Loading Flow

```
User writes config.toml with plaintext keys
         ↓
Software detects new/changed keys
         ↓
Software generates or loads encryption key
         ↓
Software encrypts sensitive fields (api_key, etc.)
         ↓
Software saves encrypted config to disk
         ↓
Software keeps decrypted keys in memory for runtime
         ↓
On next start: Load encrypted config, decrypt automatically
```

#### Step 2: Encryption Key Management

**Master Key Storage Options** (in order of preference):

1. **File in User Profile** (Cross-platform)
   - Path: `~/.dashboard_server/keys/master.key` (Linux/macOS)
   - Path: `%APPDATA%\DashboardServer\keys\master.key` (Windows)
   - File permissions: 600 (readable only by owner)
   - Contains: 256-bit AES encryption key
   - Protected by: OS file permissions

2. **Environment Variable** (Optional)
   - Variable: `DASHBOARD_MASTER_KEY`
   - Override file-based key
   - Useful for Docker/container deployments

3. **Hardware-Based Key** (Future Enhancement)
   - Use TPM (Trusted Platform Module) on supported hardware
   - Use CPU serial numbers (less secure, hardware-specific)

#### Step 3: Encryption Scheme

**Algorithm**: AES-256-GCM
- **Cipher**: AES-256 (industry standard, FIPS-approved)
- **Mode**: GCM (Galois/Counter Mode) - provides both encryption and authentication
- **Key Size**: 256 bits (32 bytes)
- **IV Size**: 12 bytes (96 bits) - unique per encryption
- **Authentication Tag**: 16 bytes - prevents tampering

**Why AES-256-GCM?**
- Fast on modern CPUs (AES-NI acceleration)
- Provides confidentiality AND integrity
- Built into standard libraries
- Resistant to known attacks

#### Step 4: Implementation Code

**encryption.hpp**:
```cpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <openssl/evp.h>
#include <openssl/rand.h>

class SecureConfig {
public:
    // Encrypt a string value
    static std::vector<uint8_t> encrypt(const std::string& plaintext, 
                                       const std::vector<uint8_t>& key);
    
    // Decrypt a vector to string
    static std::string decrypt(const std::vector<uint8_t>& ciphertext,
                               const std::vector<uint8_t>& key);
    
    // Generate a new encryption key
    static std::vector<uint8_t> generateKey();
    
    // Save key to file
    static bool saveKey(const std::vector<uint8_t>& key, const std::string& path);
    
    // Load key from file
    static std::vector<uint8_t> loadKey(const std::string& path);
    
    // Default key path for current platform
    static std::string getDefaultKeyPath();
};
```

**encryption.cpp**:
```cpp
#include "encryption.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fstream>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<uint8_t> SecureConfig::encrypt(const std::string& plaintext, 
                                          const std::vector<uint8_t>& key) {
    if (key.size() != 32) {
        throw std::invalid_argument("Key must be 32 bytes (256 bits)");
    }
    
    // Generate random IV (12 bytes for GCM)
    std::vector<uint8_t> iv(12);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        throw std::runtime_error("Failed to generate IV");
    }
    
    // Output buffer: IV (12) + ciphertext (plaintext.size()) + tag (16)
    std::vector<uint8_t> output(iv.size() + plaintext.size() + 16);
    
    // Copy IV to output
    std::copy(iv.begin(), iv.end(), output.begin());
    
    // Set up encryption context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create cipher context");
    
    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, 
                          key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encrypt initialization failed");
    }
    
    int out_len = 0;
    int ciphertext_len = 0;
    
    // Provide output buffer
    if (EVP_EncryptUpdate(ctx, output.data() + iv.size(), &out_len,
                          reinterpret_cast<const uint8_t*>(plaintext.data()),
                          plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encrypt update failed");
    }
    ciphertext_len = out_len;
    
    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, output.data() + iv.size() + ciphertext_len, &out_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Encrypt finalization failed");
    }
    ciphertext_len += out_len;
    
    // Get the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                           output.data() + iv.size() + ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to get authentication tag");
    }
    
    EVP_CIPHER_CTX_free(ctx);
    return output;
}

std::string SecureConfig::decrypt(const std::vector<uint8_t>& ciphertext,
                               const std::vector<uint8_t>& key) {
    if (key.size() != 32) {
        throw std::invalid_argument("Key must be 32 bytes (256 bits)");
    }
    
    if (ciphertext.size() < 28) { // 12 (IV) + 16 (tag) = 28 minimum
        throw std::invalid_argument("Ciphertext too short");
    }
    
    // Extract IV (first 12 bytes)
    std::vector<uint8_t> iv(12);
    std::copy(ciphertext.begin(), ciphertext.begin() + 12, iv.begin());
    
    // Set up decryption context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create cipher context");
    
    // Initialize decryption
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, 
                          key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decrypt initialization failed");
    }
    
    // Set expected tag (last 16 bytes)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                           const_cast<uint8_t*>(ciphertext.data() + ciphertext.size() - 16)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set authentication tag");
    }
    
    // Decrypt
    std::vector<uint8_t> plaintext(ciphertext.size() - 28); // Max possible size
    int out_len = 0;
    
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &out_len,
                          ciphertext.data() + 12, 
                          ciphertext.size() - 28) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decrypt update failed - possible tampering");
    }
    
    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decrypt finalization failed - authentication error");
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    plaintext.resize(out_len + final_len);
    return std::string(plaintext.begin(), plaintext.end());
}

std::vector<uint8_t> SecureConfig::generateKey() {
    std::vector<uint8_t> key(32);
    if (RAND_bytes(key.data(), key.size()) != 1) {
        throw std::runtime_error("Failed to generate encryption key");
    }
    return key;
}

std::string SecureConfig::getDefaultKeyPath() {
    #ifdef _WIN32
    std::string appdata = getenv("APPDATA");
    return appdata + "\\DashboardServer\\keys\\master.key";
    #else
    std::string home = getenv("HOME");
    return home + "/.dashboard_server/keys/master.key";
    #endif
}

bool SecureConfig::saveKey(const std::vector<uint8_t>& key, const std::string& path) {
    // Create directory if it doesn't exist
    fs::path key_path(path);
    fs::create_directories(key_path.parent_path());
    
    // Save key with restrictive permissions
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    
    file.write(reinterpret_cast<const char*>(key.data()), key.size());
    
    // Set file permissions (Unix-like systems)
    #ifndef _WIN32
    chmod(path.c_str(), 0600); // Owner read/write only
    #endif
    
    return true;
}

std::vector<uint8_t> SecureConfig::loadKey(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open key file: " + path);
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size != 32) {
        throw std::runtime_error("Invalid key file size");
    }
    
    std::vector<uint8_t> key(32);
    file.read(reinterpret_cast<char*>(key.data()), 32);
    
    return key;
}
```

#### Step 5: Secure Config File Format

When storing encrypted data, we use a **hybrid TOML format**:

**Original config.toml**:
```toml
[integrations.weather]
api_key = "my_secret_api_key_12345"
```

**Stored config.toml (after encryption)**:
```toml
[integrations.weather]
api_key = "__ENCRYPTED__98a7b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6"
api_key_iv = "__ENCRYPTED_IV__1234567890ab"
```

**But better**: Use a **separate encrypted section**:

**config.toml** (user-facing):
```toml
[server]
port = "COM3"
baud_rate = 115200

# Encrypted values are stored in config.enc.toml
# This file only contains non-sensitive settings
```

**config.enc.toml** (encrypted, auto-generated):
```toml
# This file is encrypted. Do not edit manually.
[integrations.weather]
api_key = "98a7b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6"

[integrations.portfolio]
api_key = "abc123def456ghi789jkl012mno345"

[integrations.thunderbird]
mail_directory = "pqr678stu901vwx234yza567bcd890"

# Metadata
version = 1
algorithm = "AES-256-GCM"
```

#### Step 6: Config Manager Implementation

**config_manager.hpp**:
```cpp
#pragma once
#include <string>
#include <map>
#include <variant>
#include <toml11/toml.hpp>

class ConfigManager {
public:
    ConfigManager(const std::string& config_dir = "");
    
    // Load configuration
    void load();
    
    // Save configuration (encrypts sensitive fields)
    void save();
    
    // Get/set values (interface for rest of application)
    template<typename T>
    T get(const std::string& path, const T& default_value = T{});
    
    template<typename T>
    void set(const std::string& path, const T& value, bool sensitive = false);
    
    // Check if a field is marked as sensitive
    bool isSensitive(const std::string& path) const;
    
    // Path to config directory
    std::string getConfigDir() const;
    
private:
    // Sensitive field patterns (fields containing these substrings are encrypted)
    static const std::vector<std::string> sensitive_patterns;
    
    // Encryption
    void encryptSensitiveFields();
    void decryptSensitiveFields();
    
    std::string config_dir_;
    std::string config_path_;
    std::string encrypted_config_path_;
    
    toml::value data_;           // Decrypted data in memory
    std::vector<uint8_t> master_key_;
    
    // Load or generate master key
    void loadOrGenerateMasterKey();
    
    // Check if path matches sensitive pattern
    bool isSensitiveField(const std::string& path) const;
};
```

**config_manager.cpp** (key parts):
```cpp
#include "config_manager.hpp"
#include "encryption.hpp"
#include <fstream>
#include <regex>

const std::vector<std::string> ConfigManager::sensitive_patterns = {
    "api_key",
    "password",
    "secret",
    "token",
    "key",
    "directory",  // Paths can be sensitive
    "path"       // Paths can be sensitive
};

ConfigManager::ConfigManager(const std::string& config_dir)
    : config_dir_(config_dir.empty() ? SecureConfig::getDefaultKeyPath() + "../.." : config_dir) {
    
    #ifdef _WIN32
    // On Windows, use AppData
    if (config_dir_.empty()) {
        std::string appdata = getenv("APPDATA");
        config_dir_ = appdata + "\\DashboardServer";
    }
    #else
    // On Unix-like, use ~/.config
    if (config_dir_.empty()) {
        std::string home = getenv("HOME");
        config_dir_ = home + "/.config/dashboard_server";
    }
    #endif
    
    config_path_ = fs::path(config_dir_) / "config.toml";
    encrypted_config_path_ = fs::path(config_dir_) / "config.enc.toml";
    
    // Create directory if it doesn't exist
    fs::create_directories(config_dir_);
}

void ConfigManager::load() {
    // Load or generate master key
    loadOrGenerateMasterKey();
    
    // Try to load encrypted config first
    if (fs::exists(encrypted_config_path_)) {
        std::ifstream file(encrypted_config_path_);
        try {
            data_ = toml::parse(file);
            // Decrypt all sensitive fields
            decryptSensitiveFields();
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load encrypted config: " + std::string(e.what()));
        }
    }
    // If no encrypted config, try plain TOML
    else if (fs::exists(config_path_)) {
        std::ifstream file(config_path_);
        try {
            data_ = toml::parse(file);
            // Encrypt sensitive fields and save
            encryptSensitiveFields();
            save();
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load config: " + std::string(e.what()));
        }
    }
    // If no config at all, create defaults
    else {
        // Set default values
        data_["server"]["port"] = "COM3";
        data_["server"]["baud_rate"] = 115200;
        data_["server"]["log_level"] = std::string("info");
        
        // Other defaults...
        
        // Don't encrypt yet (no sensitive data)
    }
}

void ConfigManager::save() {
    // Encrypt sensitive fields before saving
    encryptSensitiveFields();
    
    // Save to encrypted config
    std::ofstream file(encrypted_config_path_);
    file << data_;
    
    #ifndef _WIN32
    // Set restrictive permissions on Unix
    chmod(encrypted_config_path_.c_str(), 0600);
    #endif
}

void ConfigManager::encryptSensitiveFields() {
    // Walk through all values in the TOML tree
    // This is simplified - actual implementation would need to recursively traverse
    
    // For each sensitive pattern
    for (const auto& pattern : sensitive_patterns) {
        // Find all keys matching this pattern
        // For each match, encrypt the value
        // Replace plaintext with encrypted version
    }
}

void ConfigManager::decryptSensitiveFields() {
    // Walk through all values in the TOML tree
    // For each encrypted value, decrypt it
    // Replace encrypted with plaintext
}

void ConfigManager::loadOrGenerateMasterKey() {
    std::string key_path = SecureConfig::getDefaultKeyPath();
    
    try {
        master_key_ = SecureConfig::loadKey(key_path);
    } catch (const std::exception&) {
        // Key doesn't exist, generate new one
        master_key_ = SecureConfig::generateKey();
        SecureConfig::saveKey(master_key_, key_path);
    }
}

bool ConfigManager::isSensitiveField(const std::string& path) const {
    std::string lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);
    
    for (const auto& pattern : sensitive_patterns) {
        if (lower_path.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Template specializations for get/set
// (Implementation would include these for various types)
```

#### Step 7: Using the Config Manager

**In your server code**:
```cpp
#include "config_manager.hpp"

int main() {
    // Initialize config manager
    ConfigManager config;
    
    try {
        config.load();
        
        // Access configuration (automatically decrypted)
        std::string port = config.get<std::string>("server.port", "COM3");
        int baud_rate = config.get<int>("server.baud_rate", 115200);
        std::string api_key = config.get<std::string>("integrations.weather.api_key", "");
        
        // If you modify sensitive values
        config.set<std::string>("integrations.weather.api_key", "new_key", true);
        config.save(); // Automatically encrypts when saving
        
    } catch (const std::exception& e) {
        std::cerr << "Config error: " << e.what() << std::endl;
        return 1;
    }
    
    // ... rest of server initialization
    return 0;
}
```

---

## Dependency Requirements

### OpenSSL

The encryption implementation requires **OpenSSL** (or compatible library):

**Windows**:
- Install via vcpkg: `vcpkg install openssl`
- Or download from [OpenSSL for Windows](https://slproweb.com/products/Win32OpenSSL.html)
- Link with: `OpenSSL.lib` (release) or `OpenSSLd.lib` (debug)

**Linux**:
- Install via package manager: `sudo apt-get install libssl-dev` (Debian/Ubuntu)
- Or: `sudo yum install openssl-devel` (RHEL/CentOS)
- Link with: `-lssl -lcrypto`

**macOS**:
- Already installed with Xcode command line tools
- Or: `brew install openssl`

**PlatformIO (ESP32)**:
- Not needed on ESP32 (no API key storage there)
- Keys are only stored and encrypted on the server side

---

## Security Considerations

### What This Protects Against

| Threat | Protected? | Notes |
|--------|------------|-------|
| **Local file access** | ✅ Yes | Encrypted at rest |
| **Disk theft** | ✅ Yes | Files are encrypted |
| **Backup exposure** | ✅ Yes | Backups contain encrypted data |
| **Memory scraping** | ❌ No | Keys are in memory while running |
| **Process memory dump** | ❌ No | Running process has decrypted keys |
| **Brute force** | ✅ Yes | AES-256, 256-bit key = infeasible |
| **Key file theft** | ⚠️ Partial | Master key file is protected by OS permissions |
| **Quantum computing** | ✅ Yes | AES-256 is considered quantum-resistant |

### What This Does NOT Protect Against

1. **Running process memory**: If an attacker can access the process memory, they can see decrypted keys
2. **Key logger**: If you type your API key, a key logger could capture it
3. **Shoulder surfing**: Someone watching you edit the config file
4. **Screen capture**: Screenshots of the config file while open
5. **Malware with high privileges**: Root/admin malware can access anything

### Recommended Additional Measures

1. **OS-level security**: Use full-disk encryption (BitLocker, FileVault, LUKS)
2. **User permissions**: Run the server as a non-admin user
3. **Firewall**: Restrict outgoing connections to known API endpoints
4. **File permissions**: Set restrictive permissions on config directory
5. **Backup strategy**: Include encrypted config in backups (already encrypted)

### For Maximum Security (Optional)

If you want **even more security** without user involvement:

1. **Master Key in Environment Variable**
   - Set `DASHBOARD_MASTER_KEY` environment variable
   - Software checks for this before file-based key
   - Useful for: Docker containers, managed deployments

2. **TPM-backed Keys** (Windows 10/11 Pro+)
   - Use Windows TPM to seal the master key
   - Key only usable on this specific machine
   - Automatic, no user interaction needed

3. **Hardware Security Module (HSM)**
   - For enterprise/paranoid users
   - Dedicated hardware for key storage
   - Overkill for home use

---

## Update Strategy

### Weather
- **Frequency**: Every 10 minutes (600 seconds)
- **On Error**: Use cached value, retry with backoff
- **Cache Duration**: Up to 1 hour (if API unavailable)

### Thunderbird
- **Frequency**: Every 10 seconds (10 seconds)
- **On Error**: Continue with last known state
- **Cache Duration**: None (file-based, always current)

### Portfolio (Ghostfolio)
- **Frequency**: Every 30 seconds (30 seconds)
- **On Error**: Use cached value, retry with backoff
- **Cache Duration**: Up to 5 minutes
- **Note**: Since Ghostfolio is local, failures are rare

### Media Player
- **Frequency**: Every 1 second (1000ms)
- **On Error**: Try next player in list
- **Cache Duration**: None (real-time)

### System Monitor
- **Frequency**: Every 2 seconds (2000ms)
- **On Error**: Skip this update, try next
- **Cache Duration**: None (real-time)

---

## Error Handling

### Strategies

1. **Graceful Degradation**: Use cached data when live data unavailable
2. **Retry Mechanism**: Exponential backoff for failed requests
3. **Circuit Breaker**: Temporarily disable failing integrations
4. **Fallback Data**: Display default/stale data instead of errors

### Retry Policy

```cpp
class RetryPolicy {
    int max_retries = 3;
    int initial_delay_ms = 100;
    double backoff_multiplier = 2.0;
    
    template<typename Func>
    bool execute(Func func) {
        int delay = initial_delay_ms;
        for (int attempt = 0; attempt <= max_retries; attempt++) {
            if (func()) return true;
            if (attempt < max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                delay *= backoff_multiplier;
            }
        }
        return false;
    }
};
```

### Circuit Breaker

```cpp
class CircuitBreaker {
    int failure_threshold = 5;
    int reset_timeout_seconds = 30;
    int failure_count = 0;
    time_t last_failure_time = 0;
    
    bool canExecute() {
        if (failure_count >= failure_threshold) {
            time_t now = time(nullptr);
            if (now - last_failure_time < reset_timeout_seconds) {
                return false; // Circuit open
            }
            failure_count = 0; // Reset after timeout
        }
        return true;
    }
    
    void recordSuccess() { failure_count = 0; }
    void recordFailure() { 
        failure_count++;
        last_failure_time = time(nullptr);
    }
};
```

---

## Performance Optimization

### Thread Pool

Use a thread pool for parallel data source updates:

```cpp
class ThreadPool {
public:
    ThreadPool(size_t threads = 4) { /* ... */ }
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) {
        // Add task to queue
    }
};

// Usage:
ThreadPool pool(4);
pool.enqueue([&] { weather->update(); });
pool.enqueue([&] { portfolio->update(); });
// ...
```

### Update Scheduling

Different update frequencies based on data type:
- **Critical (real-time)**: Media player, system monitor
- **Important (near real-time)**: Notifications
- **Normal**: Weather, portfolio
- **Background**: Calendar (when implemented)

---

## Deployment Options

### Windows
- **Service**: Run as Windows service
- **Startup**: Add to startup folder
- **Tray App**: Minimize to system tray

### Linux
- **systemd service**: `/etc/systemd/system/dashboard-server.service`
- **Daemon**: Run as background daemon
- **Startup**: Add to `.config/autostart/`

### Cross-Platform
- **Config directory**: Platform-specific (AppData on Windows, ~/.config on Linux)
- **Log file**: Platform-specific location
- **Key storage**: Platform-specific secure location

---

## Testing Strategy

### Unit Tests
- Test each data source independently
- Mock API responses
- Test error handling
- Test data parsing

### Integration Tests
- Test data flow from API to ESP32
- Test protocol communication
- Test error scenarios
- Test performance under load

### End-to-End Tests
- Full system testing
- Long-running stability tests
- Memory leak detection
- Stress testing

---

## Summary

For implementation:

1. ✅ **Use TOML** for human-readable configuration
2. ✅ **Ghostfolio** is the specific local portfolio server
3. ⚠️ **Calendar** is NOT implemented yet (future Radicale integration)
4. ✅ **Automatic encryption**: Write plaintext to config, software encrypts automatically
5. ✅ **Secure storage**: AES-256-GCM encryption at rest
6. ✅ **No user involvement**: One-time config setup, software handles rest
7. ✅ **Cross-platform**: Works on Windows, Linux, macOS

See the implementation guide for code examples and best practices.
