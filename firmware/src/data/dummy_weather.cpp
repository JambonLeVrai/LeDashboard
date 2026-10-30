#include <stdint.h>
#include "data/weather_data.h"

// Default weather data
static WeatherData currentWeather = {
    0,  // timestamp (not used in dummy mode)
    {
        22.5f,       // temperature
        22.5f,       // feels_like
        65,         // humidity
        1013.0f,    // pressure
        5.2f,       // wind_speed
        180,        // wind_direction
        COND_CLEAR_DAY,  // condition_id (clear day)
        0,          // sunrise (placeholder)
        0           // sunset (placeholder)
    },
    {   // forecast array for 5 days
        {0, 15.0f, 25.0f, COND_CLEAR_DAY},
        {0, 16.0f, 26.0f, COND_PARTLY_CLOUDY},
        {0, 14.0f, 24.0f, COND_CLOUDY},
        {0, 13.0f, 23.0f, COND_RAIN_LIGHT},
        {0, 12.0f, 22.0f, COND_CLEAR_DAY}
    },
    5,              // forecast_count
    25,             // air_quality (25 = Good)
    5               // uv_index (5 = Moderate)
};

const WeatherData& getCurrentWeather() {
    return currentWeather;
}

void updateWeatherData(const WeatherData& newWeather) {
    currentWeather = newWeather;
}
