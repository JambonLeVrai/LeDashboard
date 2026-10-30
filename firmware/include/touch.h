/*******************************************************************************
 * Touch libraries:
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 ******************************************************************************/

#define TOUCH_GT911

// Touch pins for ESP32-4848S040
#define TOUCH_GT911_SCL 45
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_INT -1
#define TOUCH_GT911_RST -1
#define TOUCH_GT911_ROTATION ROTATION_NORMAL

// Touch coordinate mapping
#define TOUCH_MAP_X1 480
#define TOUCH_MAP_X2 0
#define TOUCH_MAP_Y1 480
#define TOUCH_MAP_Y2 0

#include <stdint.h>
#include <Wire.h>
#include <Touch_GT911.h>

// Touch variables
int16_t touch_last_x = 0, touch_last_y = 0;

// Touch screen instance
Touch_GT911 ts = Touch_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

void touch_init()
{
  Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
  ts.begin();
  ts.setRotation(TOUCH_GT911_ROTATION);
}

bool touch_has_signal()
{
  return true;
}

bool touch_touched()
{
  ts.read();
  if (ts.isTouched)
  {
    touch_last_x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 480 - 1);
    touch_last_y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 480 - 1);
    return true;
  }
  else
  {
    return false;
  }
}

bool touch_released()
{
    return true;
}
