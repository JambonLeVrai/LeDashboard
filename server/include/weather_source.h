#pragma once

#include "protocol.h"
#include <string>
#include <chrono>

// =============================================================================
// WeatherSource — fetches weather from Open-Meteo (no API key required)
// =============================================================================

class WeatherSource {
public:
    // Initialize with a city name (geocoded via Open-Meteo) or "lat,lon".
    bool initialize(const std::string &location);

    // Fetch latest weather. Returns true on success.
    bool update();

    // Get the last fetched weather as a binary message ready to send.
    const WeatherMessageBinary &last_message() const { return last_msg_; }
    bool has_data() const { return has_data_; }
    std::string last_error() const { return last_error_; }

    // How often to refetch (default 10 minutes)
    void set_update_interval(std::chrono::seconds interval) { update_interval_ = interval; }
    bool needs_update() const;

private:
    // Geocode city name → lat/lon via Open-Meteo geocoding API
    bool geocode(const std::string &location);

    // Fetch current + daily forecast
    bool fetch_forecast();

    // Fetch air quality (European AQI)
    bool fetch_air_quality();

    // Map WMO weather code to our COND_* condition ID
    static uint8_t map_wmo_code(int wmo_code);

    double lat_ = 0.0;
    double lon_ = 0.0;
    std::string location_name_;

    WeatherMessageBinary last_msg_{};
    bool has_data_ = false;
    std::string last_error_;

    std::chrono::steady_clock::time_point last_update_;
    std::chrono::seconds update_interval_{600}; // 10 min default
};
