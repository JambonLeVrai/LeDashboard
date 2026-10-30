#include "ui/time_weather_page.h"
#include "protocol.h"
#include <stdio.h>

TimeWeatherPage::TimeWeatherPage() 
    : PageBase(), timeData(getCurrentTime()), weatherData(getCurrentWeather()) {
    // Initialize member variables
    dateLabel = nullptr;
    timeLabel = nullptr;
    weatherIcon = nullptr;
    tempLabel = nullptr;
    humidityLabel = nullptr;
    aqiLabel = nullptr;
    uvLabel = nullptr;
    tomorrowIcon = nullptr;
    tomorrowTempLabel = nullptr;
}

TimeWeatherPage::~TimeWeatherPage() {
}

void TimeWeatherPage::create(lv_obj_t* parent) {
    // Create main container for this page
    page_container = lv_obj_create(parent);
    lv_obj_set_size(page_container, 480, 480);
    lv_obj_set_pos(page_container, 0, 0);
    lv_obj_set_style_bg_opa(page_container, LV_OPA_TRANSP, 0);
    
    // Create left column container
    lv_obj_t* leftColumn = lv_obj_create(page_container);
    lv_obj_set_size(leftColumn, 240, 480);
    lv_obj_set_pos(leftColumn, 0, 0);
    lv_obj_set_style_bg_opa(leftColumn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(leftColumn, LV_OPA_TRANSP, 0);
    
    // Date label - Top left: "Jeudi 24 octobre 2026"
    dateLabel = lv_label_create(leftColumn);
    lv_obj_set_pos(dateLabel, 10, 10);
    lv_obj_set_width(dateLabel, 220);
    lv_label_set_long_mode(dateLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(dateLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(dateLabel, &lv_font_montserrat_24, 0);
    
    // Time label - Below date: big "13:40"
    timeLabel = lv_label_create(leftColumn);
    lv_obj_set_pos(timeLabel, 10, 50);
    lv_obj_set_width(timeLabel, 220);
    lv_label_set_long_mode(timeLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(timeLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
    
    // Weather icon - Below time
    weatherIcon = lv_img_create(leftColumn);
    lv_obj_set_pos(weatherIcon, 10, 130);
    lv_obj_set_size(weatherIcon, 80, 80);
    lv_img_set_zoom(weatherIcon, 256);
    
    // Temperature label - Below weather icon
    tempLabel = lv_label_create(leftColumn);
    lv_obj_set_pos(tempLabel, 10, 220);
    lv_obj_set_width(tempLabel, 220);
    lv_obj_set_style_text_color(tempLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_28, 0);
    
    // Weather details - Below temperature
    // RH, AQ, UV in smaller text
    humidityLabel = lv_label_create(leftColumn);
    lv_obj_set_pos(humidityLabel, 10, 265);
    lv_obj_set_width(humidityLabel, 220);
    lv_obj_set_style_text_color(humidityLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(humidityLabel, &lv_font_montserrat_18, 0);
    
    aqiLabel = lv_label_create(leftColumn);
    lv_obj_set_pos(aqiLabel, 10, 290);
    lv_obj_set_width(aqiLabel, 220);
    lv_obj_set_style_text_color(aqiLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(aqiLabel, &lv_font_montserrat_18, 0);
    
    uvLabel = lv_label_create(leftColumn);
    lv_obj_set_pos(uvLabel, 10, 315);
    lv_obj_set_width(uvLabel, 220);
    lv_obj_set_style_text_color(uvLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(uvLabel, &lv_font_montserrat_18, 0);
    
    // Create right column container
    lv_obj_t* rightColumn = lv_obj_create(page_container);
    lv_obj_set_size(rightColumn, 240, 480);
    lv_obj_set_pos(rightColumn, 240, 0);
    lv_obj_set_style_bg_opa(rightColumn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(rightColumn, LV_OPA_TRANSP, 0);
    
    // Tomorrow's weather icon
    tomorrowIcon = lv_img_create(rightColumn);
    lv_obj_set_pos(tomorrowIcon, 20, 130);
    lv_obj_set_size(tomorrowIcon, 64, 64);
    lv_img_set_zoom(tomorrowIcon, 256);
    
    // Tomorrow's temperature (min/max)
    tomorrowTempLabel = lv_label_create(rightColumn);
    lv_obj_set_pos(tomorrowTempLabel, 20, 210);
    lv_obj_set_width(tomorrowTempLabel, 200);
    lv_obj_set_style_text_color(tomorrowTempLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(tomorrowTempLabel, &lv_font_montserrat_24, 0);
    lv_label_set_long_mode(tomorrowTempLabel, LV_LABEL_LONG_WRAP);
    
    // Now update with actual data
    update();
}

void TimeWeatherPage::destroy() {
    if (page_container != nullptr) {
        lv_obj_del(page_container);
        page_container = nullptr;
    }
}

void TimeWeatherPage::update() {
    updateDate();
    updateTime();
    updateCurrentWeather();
    updateForecast();
}

void TimeWeatherPage::updateDate() {
    char dateBuffer[64];
    formatFrenchDate(dateBuffer, sizeof(dateBuffer));
    lv_label_set_text(dateLabel, dateBuffer);
}

void TimeWeatherPage::updateTime() {
    char timeBuffer[16];
    formatTimeString(timeBuffer, sizeof(timeBuffer));
    lv_label_set_text(timeLabel, timeBuffer);
}

void TimeWeatherPage::updateCurrentWeather() {
    // Update weather icon
    const char* iconName = getWeatherIconName(weatherData.current.condition_id);
    // For now, we'll use a placeholder image since we don't have the actual images
    // In real implementation, you'd have these images as LVGL C arrays
    // lv_img_set_src(weatherIcon, &icon_clear_day);
    
    // Update temperature
    char tempBuffer[32];
    snprintf(tempBuffer, sizeof(tempBuffer), "%.1f°C", weatherData.current.temperature);
    lv_label_set_text(tempLabel, tempBuffer);
    
    // Update humidity
    char humidityBuffer[32];
    snprintf(humidityBuffer, sizeof(humidityBuffer), "RH: %d%%", weatherData.current.humidity);
    lv_label_set_text(humidityLabel, humidityBuffer);
    
    // Update AQI
    char aqiBuffer[32];
    const char* aqiStr = get_air_quality_string(weatherData.air_quality);
    snprintf(aqiBuffer, sizeof(aqiBuffer), "QA: %s", aqiStr);
    lv_label_set_text(aqiLabel, aqiBuffer);
    
    // Update UV
    char uvBuffer[32];
    const char* uvStr = get_uv_index_string(weatherData.uv_index);
    snprintf(uvBuffer, sizeof(uvBuffer), "UV: %s", uvStr);
    lv_label_set_text(uvLabel, uvBuffer);
}

void TimeWeatherPage::updateForecast() {
    if (weatherData.forecast_count > 0 && weatherData.forecast[0].min_temp != 0) {
        // Update tomorrow's weather icon
        const char* tomorrowIconName = getWeatherIconName(weatherData.forecast[0].condition_id);
        // lv_img_set_src(tomorrowIcon, &icon_partly_cloudy); // Placeholder
        
        // Update tomorrow's temperature range
        char tempBuffer[32];
        snprintf(tempBuffer, sizeof(tempBuffer), "%.0f - %.0f°C", 
                 weatherData.forecast[0].min_temp, 
                 weatherData.forecast[0].max_temp);
        lv_label_set_text(tomorrowTempLabel, tempBuffer);
    }
}

void TimeWeatherPage::formatFrenchDate(char* buffer, unsigned int size) {
    const TimeData& time = getCurrentTime();
    
    // French day names (0=Monday, 6=Sunday)
    const char* dayNames[] = {
        "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"
    };
    
    // French month names (1-12)
    const char* monthNames[] = {
        "", "janvier", "février", "mars", "avril", "mai", "juin",
        "juillet", "août", "septembre", "octobre", "novembre", "décembre"
    };
    
    uint8_t dayOfWeek = time.day_of_week;
    if (dayOfWeek >= 7) dayOfWeek = 0; // Safety check
    
    uint8_t month = time.month;
    if (month < 1 || month > 12) month = 1; // Safety check
    
    snprintf(buffer, size, "%s %d %s %d",
             dayNames[dayOfWeek],
             time.day,
             monthNames[month],
             time.year);
}

void TimeWeatherPage::formatTimeString(char* buffer, unsigned int size) {
    const TimeData& time = getCurrentTime();
    snprintf(buffer, size, "%02d:%02d", time.hour, time.minute);
}

void TimeWeatherPage::handleTouch(lv_obj_t* target) {
    // Handle touch events on this page
    // For now, we can implement the long press on weather area to show forecast
    // But for initial implementation, we'll just ignore touch events
}
