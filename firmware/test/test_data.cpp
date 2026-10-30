#include "../include/data/time_data.h"
#include "../include/data/weather_data.h"
#include "../include/protocol.h"
#include <stdio.h>

void test_time_data() {
    printf("Testing Time Data...\n");
    
    const TimeData& time = getCurrentTime();
    printf("Time: %02d:%02d:%02d\n", time.hour, time.minute, time.second);
    printf("Date: %d/%d/%d\n", time.day, time.month, time.year);
    printf("Day of week: %d\n", time.day_of_week);
    
    // Test French formatting
    printf("French day: %s\n", DAY_NAMES_FR[time.day_of_week]);
    printf("French month: %s\n", MONTH_NAMES_FR[time.month - 1]);
}

void test_weather_data() {
    printf("\nTesting Weather Data...\n");
    
    const WeatherData& weather = getCurrentWeather();
    printf("Current temp: %.1f°C\n", weather.current.temperature);
    printf("Humidity: %d%%\n", weather.current.humidity);
    printf("Condition: %s\n", getWeatherDescription(weather.current.condition_id));
    printf("Icon: %s\n", getWeatherIconName(weather.current.condition_id));
    
    printf("\nForecast:\n");
    for (int i = 0; i < weather.forecast_count; i++) {
        printf("  Day %d: %.1f - %.1f°C, %s\n", 
               i, 
               weather.forecast[i].min_temp, 
               weather.forecast[i].max_temp,
               getWeatherDescription(weather.forecast[i].condition_id));
    }
    
    printf("\nAir Quality: %s\n", get_air_quality_string(weather.air_quality));
    printf("UV Index: %s\n", get_uv_index_string(weather.uv_index));
}

void test_protocol() {
    printf("\nTesting Protocol...\n");
    
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = crc16_ccitt(test_data, sizeof(test_data));
    printf("CRC16 of test data: 0x%04X\n", crc);
}

int main() {
    test_time_data();
    test_weather_data();
    test_protocol();
    
    printf("\nAll tests completed!\n");
    return 0;
}
