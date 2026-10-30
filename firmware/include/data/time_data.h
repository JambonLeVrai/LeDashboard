#ifndef TIME_DATA_H
#define TIME_DATA_H

#include <stdint.h>
#include <stddef.h>

// Time data structure
struct TimeData {
    uint8_t hour;         // 0-23
    uint8_t minute;       // 0-59
    uint8_t second;       // 0-59
    uint8_t day;          // 1-31
    uint8_t month;        // 1-12
    uint16_t year;        // 2000-2099
    uint8_t day_of_week;  // 0=Monday, 6=Sunday
};

// Get current time data (from dummy or real source)
const TimeData& getCurrentTime();

// Update time data
void updateTimeData(const TimeData& newTime);

#endif // TIME_DATA_H
