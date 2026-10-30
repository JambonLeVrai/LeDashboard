#include <stdint.h>
#include "data/time_data.h"

// Default time data (will be updated by dummy or real source)
static TimeData currentTime = {
    13,    // hour (1 PM)
    40,    // minute
    0,     // second
    24,    // day
    10,    // month (October)
    2026,  // year
    3      // day_of_week: Thursday (0=Monday, 6=Sunday)
};

const TimeData& getCurrentTime() {
    return currentTime;
}

void updateTimeData(const TimeData& newTime) {
    currentTime = newTime;
}
