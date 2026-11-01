#include "weather_source.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

// =============================================================================
// WMO weather code → COND_* mapping
// =============================================================================

uint8_t WeatherSource::map_wmo_code(int wmo_code) {
    switch (wmo_code) {
        case 0:  return 0;  // Clear sky → COND_CLEAR_DAY
        case 1:  return 0;  // Mainly clear → COND_CLEAR_DAY
        case 2:  return 2;  // Partly cloudy → COND_PARTLY_CLOUDY
        case 3:  return 4;  // Overcast → COND_OVERCAST
        case 45: case 48: return 10; // Fog → COND_FOG
        case 51: case 53: case 55: case 56: case 57: return 5;  // Drizzle → COND_RAIN_LIGHT
        case 61: case 63: case 65: case 66: case 67:
        case 80: case 81: case 82: return 6;  // Rain → COND_RAIN_MODERATE
        case 71: case 73: case 75: case 77: case 85: case 86: return 9; // Snow → COND_SNOW
        case 95: case 96: case 99: return 8;  // Thunderstorm → COND_THUNDERSTORM
        default: return 3;  // Cloudy → COND_CLOUDY
    }
}

// =============================================================================
// Initialization / geocoding
// =============================================================================

bool WeatherSource::initialize(const std::string &location) {
    location_name_ = location;

    // If location looks like "lat,lon", parse directly
    auto comma = location.find(',');
    if (comma != std::string::npos) {
        try {
            lat_ = std::stod(location.substr(0, comma));
            lon_ = std::stod(location.substr(comma + 1));
            spdlog::info("Weather: using coordinates lat={} lon={}", lat_, lon_);
            return true;
        } catch (...) {
            last_error_ = "Invalid coordinates: " + location;
            return false;
        }
    }

    return geocode(location);
}

bool WeatherSource::geocode(const std::string &location) {
    std::string url = "https://geocoding-api.open-meteo.com/v1/search?name=" +
                      std::string(cpr::util::urlEncode(location)) +
                      "&count=1&language=fr&format=json";

    spdlog::info("Weather: geocoding '{}'...", location);
    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});

    if (r.status_code != 200) {
        last_error_ = "Geocoding HTTP " + std::to_string(r.status_code);
        spdlog::error("Weather: {}", last_error_);
        return false;
    }

    try {
        auto j = json::parse(r.text);
        if (!j.contains("results") || j["results"].empty()) {
            last_error_ = "Location not found: " + location;
            spdlog::error("Weather: {}", last_error_);
            return false;
        }
        auto &first = j["results"][0];
        lat_ = first["latitude"].get<double>();
        lon_ = first["longitude"].get<double>();
        std::string name = first.value("name", location);
        std::string country = first.value("country", "");
        spdlog::info("Weather: {} → lat={} lon={} ({}, {})", location, lat_, lon_, name, country);
        return true;
    } catch (const std::exception &e) {
        last_error_ = std::string("Geocoding parse error: ") + e.what();
        spdlog::error("Weather: {}", last_error_);
        return false;
    }
}

// =============================================================================
// Update
// =============================================================================

bool WeatherSource::needs_update() const {
    if (!has_data_) return true;
    return std::chrono::steady_clock::now() - last_update_ >= update_interval_;
}

bool WeatherSource::update() {
    if (!fetch_forecast()) return false;
    fetch_air_quality(); // non-fatal if it fails
    last_update_ = std::chrono::steady_clock::now();
    has_data_ = true;
    return true;
}

bool WeatherSource::fetch_forecast() {
    std::string url =
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=" + std::to_string(lat_) +
        "&longitude=" + std::to_string(lon_) +
        "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,"
        "wind_speed_10m,wind_direction_10m,pressure_msl"
        "&daily=temperature_2m_max,temperature_2m_min,weather_code,uv_index_max,sunrise,sunset"
        "&timezone=auto&forecast_days=2";

    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{15000});
    if (r.status_code != 200) {
        last_error_ = "Forecast HTTP " + std::to_string(r.status_code);
        spdlog::error("Weather: {}", last_error_);
        return false;
    }

    try {
        auto j = json::parse(r.text);

        // Current
        auto &cur = j["current"];
        double temp = cur["temperature_2m"].get<double>();
        double feels = cur["apparent_temperature"].get<double>();
        int humidity = cur["relative_humidity_2m"].get<int>();
        int wmo = cur["weather_code"].get<int>();
        double wind_spd = cur.value("wind_speed_10m", 0.0);
        int wind_dir = cur.value("wind_direction_10m", 0);
        double pressure = cur.value("pressure_msl", 1013.0);

        // Daily (index 0 = today, 1 = tomorrow)
        auto &daily = j["daily"];
        double tmr_max = daily["temperature_2m_max"][1].get<double>();
        double tmr_min = daily["temperature_2m_min"][1].get<double>();
        int tmr_wmo = daily["weather_code"][1].get<int>();
        double uv_max = daily["uv_index_max"][0].get<double>(); // today's UV max

        // Build binary message
        auto &m = last_msg_;
        m.timestamp_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        m.temp_x10 = static_cast<int16_t>(std::lround(temp * 10.0));
        m.feels_like_x10 = static_cast<int16_t>(std::lround(feels * 10.0));
        m.humidity = static_cast<uint8_t>(humidity);
        m.condition_id = map_wmo_code(wmo);
        m.uv_index = static_cast<uint8_t>(std::lround(uv_max));
        m.air_quality = 0; // filled by fetch_air_quality
        m.wind_speed_x10 = static_cast<uint16_t>(std::lround(wind_spd * 10.0));
        m.wind_direction = static_cast<uint16_t>(wind_dir);
        m.tomorrow_min_x10 = static_cast<int16_t>(std::lround(tmr_min * 10.0));
        m.tomorrow_max_x10 = static_cast<int16_t>(std::lround(tmr_max * 10.0));
        m.tomorrow_condition = map_wmo_code(tmr_wmo);
        m.pressure_x10 = static_cast<uint16_t>(std::lround(pressure * 10.0));

        spdlog::info("Weather: {}°C (feels {}°C), RH {}%, wind {} m/s @ {}°, cond={}, UV={}, "
                     "tomorrow {}/{}°C cond={}",
                     temp, feels, humidity, wind_spd, wind_dir, m.condition_id,
                     m.uv_index, tmr_min, tmr_max, m.tomorrow_condition);
        return true;

    } catch (const std::exception &e) {
        last_error_ = std::string("Forecast parse error: ") + e.what();
        spdlog::error("Weather: {}", last_error_);
        return false;
    }
}

bool WeatherSource::fetch_air_quality() {
    std::string url =
        "https://air-quality-api.open-meteo.com/v1/air-quality"
        "?latitude=" + std::to_string(lat_) +
        "&longitude=" + std::to_string(lon_) +
        "&current=european_aqi";

    auto r = cpr::Get(cpr::Url{url}, cpr::Timeout{10000});
    if (r.status_code != 200) {
        spdlog::warn("Weather: air quality HTTP {} (non-fatal)", r.status_code);
        return false;
    }

    try {
        auto j = json::parse(r.text);
        if (j.contains("current") && j["current"].contains("european_aqi")) {
            double aqi = j["current"]["european_aqi"].get<double>();
            last_msg_.air_quality = static_cast<uint16_t>(std::lround(aqi));
            spdlog::info("Weather: AQI (EU) = {}", last_msg_.air_quality);
        }
        return true;
    } catch (const std::exception &e) {
        spdlog::warn("Weather: AQI parse error: {}", e.what());
        return false;
    }
}
