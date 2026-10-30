#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H

#include <stdint.h>
#include <stddef.h>

// Weather condition IDs
#define COND_CLEAR_DAY         0
#define COND_CLEAR_NIGHT       1
#define COND_PARTLY_CLOUDY     2
#define COND_CLOUDY           3
#define COND_OVERCAST         4
#define COND_RAIN_LIGHT       5
#define COND_RAIN_MODERATE    6
#define COND_RAIN_HEAVY       7
#define COND_THUNDERSTORM     8
#define COND_SNOW             9
#define COND_FOG             10
#define COND_MIST            11
#define COND_WINDY           12

// Current weather data
struct CurrentWeather {
    float temperature;      // °C
    float feels_like;       // °C
    uint8_t humidity;       // 0-100%
    float pressure;         // hPa
    float wind_speed;       // m/s
    uint16_t wind_direction; // 0-359°
    uint8_t condition_id;    // See COND_* defines
    uint32_t sunrise;        // Unix timestamp
    uint32_t sunset;         // Unix timestamp
};

// Forecast day data
struct WeatherForecastDay {
    uint32_t timestamp;
    float min_temp;       // °C
    float max_temp;       // °C
    uint8_t condition_id; // See COND_* defines
};

// Full weather data
struct WeatherData {
    uint32_t timestamp;
    CurrentWeather current;
    WeatherForecastDay forecast[5]; // Next 5 days
    uint8_t forecast_count;
    
    // Air quality index (0-500, typical range)
    uint16_t air_quality;
    
    // UV index (0-11+)
    uint8_t uv_index;
};

// Get current weather data
const WeatherData& getCurrentWeather();

// Update weather data
void updateWeatherData(const WeatherData& newWeather);

// Get weather condition icon name
const char* getWeatherIconName(uint8_t condition_id);

// Get weather condition description
const char* getWeatherDescription(uint8_t condition_id);

#endif // WEATHER_DATA_H
