#pragma once

#include "protocol.h"
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>

// =============================================================================
// WeatherSource — fetches weather from Open-Meteo (no API key required)
// =============================================================================

class WeatherSource {
public:
    bool initialize(const std::string &location);

    // Start background fetch thread (fetches immediately, then every hour)
    void start();
    void stop();

    // Thread-safe snapshot of last fetched weather
    WeatherMessageBinary last_message() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_msg_;
    }
    bool has_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return has_data_;
    }
    std::string last_error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }
    // True if new data arrived since last clear_dirty()
    bool dirty() const { return dirty_.load(); }
    void clear_dirty() { dirty_ = false; }

private:
    void fetch_loop();
    bool do_update();

    bool geocode(const std::string &location);
    bool fetch_forecast();
    bool fetch_air_quality();
    static uint8_t map_wmo_code(int wmo_code);

    double lat_ = 0.0;
    double lon_ = 0.0;
    std::string location_name_;

    mutable std::mutex mutex_;
    WeatherMessageBinary last_msg_{};
    bool has_data_ = false;
    std::string last_error_;
    std::atomic<bool> dirty_{true};

    std::atomic<bool> running_{false};
    std::thread thread_;

    static constexpr auto FETCH_INTERVAL = std::chrono::hours(1);
};
