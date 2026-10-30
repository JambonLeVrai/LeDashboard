#ifndef TIME_WEATHER_PAGE_H
#define TIME_WEATHER_PAGE_H

#include "page_base.h"
#include "../data/time_data.h"
#include "../data/weather_data.h"

class TimeWeatherPage : public PageBase {
public:
    TimeWeatherPage();
    ~TimeWeatherPage();
    
    void create(lv_obj_t* parent) override;
    void destroy() override;
    void update() override;
    
    // Handle touch events
    void handleTouch(lv_obj_t* target);

private:
    // UI Elements - Left column
    lv_obj_t* dateLabel;      // "Jeudi 24 octobre 2026"
    lv_obj_t* timeLabel;      // "13:40" (big)
    lv_obj_t* weatherIcon;    // Weather symbol
    lv_obj_t* tempLabel;      // Current temperature
    lv_obj_t* humidityLabel;  // RH (Relative Humidity)
    lv_obj_t* aqiLabel;       // Air Quality
    lv_obj_t* uvLabel;        // UV Index
    
    // UI Elements - Right column
    lv_obj_t* tomorrowIcon;   // Tomorrow's weather symbol
    lv_obj_t* tomorrowTempLabel; // Min/Max temperature for tomorrow
    
    // Data references
    const TimeData& timeData;
    const WeatherData& weatherData;
    
    // Update specific parts
    void updateDate();
    void updateTime();
    void updateCurrentWeather();
    void updateForecast();
    
    // Format date string in French
    void formatFrenchDate(char* buffer, unsigned int size);
    
    // Format time string
    void formatTimeString(char* buffer, unsigned int size);
};

#endif // TIME_WEATHER_PAGE_H
