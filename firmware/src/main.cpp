// ESP32-4848S040 Touch Display with LVGL - Time & Weather Dashboard
// Page 1: Combined Time + Weather display with French formatting

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP32_4848S040.h>
#include <lvgl.h>
#include "touch.h"
#include "protocol.h"
#include "serial_handler.h"
#include "fonts/montserrat_96.h"
#include "fonts/song_title_fallback_18.h"
#include "icons/icons.h"
#include "backgrounds/background.h"

// Title font with CJK fallback chained
static lv_font_t g_title_font;

// Display backlight pin
#define GFX_BL 38

// Display objects
Arduino_ESP32SPI *bus;
Arduino_RGB_Display *gfx;

// LVGL buffer definitions
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
static uint8_t buf1[480 * 480 / 10 * BYTE_PER_PIXEL];
lv_display_t *display;

// LVGL display configuration
#define TFT_HOR_RES   480
#define TFT_VER_RES   480
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0
#define TFT_BRIGHTNESS 255

// LVGL draw buffer: 1/10 screen size usually works well (size is in bytes)
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// Dummy time data structure
struct TimeData {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t day_of_week; // 0=Monday, 1=Tuesday, ... 6=Sunday
};

// Dummy weather data structure
struct WeatherData {
    float current_temp;
    float tomorrow_min;
    float tomorrow_max;
    const char* condition;
    uint8_t humidity;
    uint8_t air_quality; // 0-5 scale for French QA
    uint8_t uv_index;
    uint8_t condition_id;       // current weather condition (COND_*)
    uint8_t tomorrow_condition; // tomorrow condition (COND_*)
};

// French day and month names
const char* french_days[] = {"lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi", "dimanche"};
const char* french_months[] = {
    "janvier", "février", "mars", "avril", "mai", "juin",
    "juillet", "août", "septembre", "octobre", "novembre", "décembre"
};

// Global time data (will be updated from server later)
TimeData current_time;
WeatherData current_weather;

// True once we receive the first TIME message from the server
static bool time_synced_from_server = false;

// Millisecond-precision sync state:
// When a TIME message arrives, we record the server's UTC timestamp (ms) and
// our local millis() at reception. Current server time is then:
//   server_now_ms = server_ts_ms + (millis() - local_rx_millis)
static uint64_t server_ts_ms = 0;       // server UTC timestamp in ms
static int16_t  server_tz_min = 0;       // server timezone offset (minutes east of UTC)
static uint32_t local_rx_millis = 0;     // millis() when last TIME was received

// Global label references for updating
lv_obj_t *time_label = NULL;
lv_obj_t *date_label = NULL;

// Weather UI elements (set in create_time_weather_page)
lv_obj_t *weather_icon = NULL;
lv_obj_t *temp_label = NULL;
lv_obj_t *rh_label = NULL;
lv_obj_t *aq_label = NULL;
lv_obj_t *uv_label = NULL;
lv_obj_t *tomorrow_icon = NULL;
lv_obj_t *tomorrow_temp = NULL;

// Forward declarations
void update_time_display();
void update_date_display();
void format_date_single_line(char* buffer, size_t size);
void on_server_time(const TimeMessageBinary *t);
void on_server_weather(const WeatherMessageBinary *w);
void on_server_media(const MediaMessageBinary *hdr, const char *title, const char *artist);
void on_conn_state(connection_state_t s);
static void on_tap_detected();
static lv_obj_t *g_page1;

// Display flushing callback for LVGL
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#endif

  lv_disp_flush_ready(disp);
}

// Touchpad input callback for LVGL
static bool g_prev_touch = false;
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      g_prev_touch = true;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_RELEASED;
      // Tap detected on release — check for double-tap
      if (g_prev_touch) {
        g_prev_touch = false;
        on_tap_detected();
      }
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
    g_prev_touch = false;
  }
}

// Tick callback for LVGL
static uint32_t my_tick(void)
{
  return millis();
}

// Update time display labels from current_time
void update_time_display() {
  if (time_label != NULL) {
    char time_str[10];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", current_time.hour, current_time.minute);
    lv_label_set_text(time_label, time_str);
  }
}

void update_date_display() {
  if (date_label != NULL) {
    char date_str[40];
    format_date_single_line(date_str, sizeof(date_str));
    lv_label_set_text(date_label, date_str);
  }
}

// Called by SerialHandler when a TIME message arrives from the server.
// Uses the millisecond timestamp for sub-second sync precision.
void on_server_time(const TimeMessageBinary *t) {
  // Record sync anchor: server UTC ms + our local millis at reception
  server_ts_ms     = t->timestamp_ms;
  server_tz_min    = t->timezone_min;
  local_rx_millis  = millis();

  // Also set the display fields from the pre-computed local time
  current_time.hour        = t->hour;
  current_time.minute      = t->minute;
  current_time.second      = t->second;
  current_time.day         = t->day;
  current_time.month       = t->month;
  current_time.year        = t->year;
  current_time.day_of_week = t->day_of_week;
  time_synced_from_server  = true;
  update_time_display();
  update_date_display();
}

// Compute current server-local time in milliseconds since Unix epoch,
// using the ms-precision anchor. Returns ms since epoch in local time.
static uint64_t get_server_local_ms() {
  uint64_t utc_now = server_ts_ms + (uint64_t)(millis() - local_rx_millis);
  // Apply timezone offset (minutes east of UTC → ms)
  return utc_now + (uint64_t)server_tz_min * 60ULL * 1000ULL;
}

// Convert Unix epoch ms (local) to broken-down time fields.
// Uses a simple civil-from-days algorithm (no libc time functions needed).
static void ms_to_datetime(uint64_t epoch_ms, TimeData *out) {
  // Days since 1970-01-01
  uint64_t days = epoch_ms / 86400000ULL;
  uint64_t rem_ms = epoch_ms % 86400000ULL;

  out->hour   = (uint8_t)(rem_ms / 3600000ULL);
  out->minute = (uint8_t)((rem_ms % 3600000ULL) / 60000ULL);
  out->second = (uint8_t)((rem_ms % 60000ULL) / 1000ULL);

  // Civil from days (Howard Hinnant's algorithm)
  // Shift epoch to 1970-03-01 to simplify leap year handling
  int64_t z = (int64_t)days + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);          // [0, 146096]
  uint32_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365; // [0, 399]
  int64_t y = (int64_t)yoe + era * 400;
  uint32_t doy = doe - (365*yoe + yoe/4 - yoe/100);     // [0, 365]
  uint32_t mp = (5*doy + 2)/153;                         // [0, 11]
  uint32_t d = doy - (153*mp+2)/5 + 1;                   // [1, 31]
  uint32_t m = mp < 10 ? mp+3 : mp-9;                    // [1, 12]
  if (m <= 2) y += 1;

  out->day   = (uint8_t)d;
  out->month = (uint8_t)m;
  out->year  = (uint16_t)y;
  // Day of week: 1970-01-01 was Thursday (4). 0=Monday..6=Sunday
  out->day_of_week = (uint8_t)((days + 3) % 7);
}

// Called when connection state changes
void on_conn_state(connection_state_t s) {
  const char *names[] = {"IDLE", "CONNECTED", "LOST"};
  Serial.printf("[MAIN] Server connection: %s\n", names[(int)s]);
}

// Map condition_id to the 96px weather icon
static const lv_image_dsc_t *condition_to_icon(uint8_t cond) {
  switch (cond) {
    case 0: case 1: return &sun_96;            // clear
    case 2:         return &partly_cloudy_96;  // partly cloudy
    case 3:         return &cloudy_96;         // cloudy
    case 4:         return &cloudy_96;         // overcast → cloudy icon
    case 5:         return &drizzle_96;        // drizzle / light rain
    case 6: case 7: return &rain_96;           // rain
    case 8:         return &thunder_96;        // thunderstorm
    case 9:         return &snow_96;           // snow
    case 10: case 11: return &fog_96;          // fog / mist
    case 12:        return &wind_96;           // windy
    default:        return &partly_cloudy_96;
  }
}

// Called by SerialHandler when a WEATHER message arrives from the server.
// Only updates LVGL objects when values actually changed (avoids re-render lag).
void on_server_weather(const WeatherMessageBinary *w) {
  // Current conditions — compare before touching LVGL
  if (weather_icon && current_weather.condition_id != w->condition_id) {
    lv_image_set_src(weather_icon, condition_to_icon(w->condition_id));
  }
  float new_temp = w->temp_x10 / 10.0f;
  if (temp_label && current_weather.current_temp != new_temp) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f°C", new_temp);
    lv_label_set_text(temp_label, buf);
  }
  if (rh_label && current_weather.humidity != w->humidity) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", w->humidity);
    lv_label_set_text(rh_label, buf);
  }
  uint16_t new_aqi = w->air_quality;
  if (aq_label && current_weather.air_quality != (new_aqi > 255 ? 255 : new_aqi)) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", new_aqi);
    lv_label_set_text(aq_label, buf);
  }
  if (uv_label && current_weather.uv_index != w->uv_index) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", w->uv_index);
    lv_label_set_text(uv_label, buf);
  }

  // Tomorrow forecast
  if (tomorrow_icon && current_weather.tomorrow_condition != w->tomorrow_condition) {
    lv_image_set_src(tomorrow_icon, condition_to_icon(w->tomorrow_condition));
  }
  float new_tmr_max = w->tomorrow_max_x10 / 10.0f;
  float new_tmr_min = w->tomorrow_min_x10 / 10.0f;
  if (tomorrow_temp && (current_weather.tomorrow_max != new_tmr_max ||
                        current_weather.tomorrow_min != new_tmr_min)) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f/%.0f°C", new_tmr_max, new_tmr_min);
    lv_label_set_text(tomorrow_temp, buf);
  }

  // Store for next comparison
  current_weather.current_temp     = new_temp;
  current_weather.tomorrow_min     = new_tmr_min;
  current_weather.tomorrow_max     = new_tmr_max;
  current_weather.condition_id     = w->condition_id;
  current_weather.tomorrow_condition = w->tomorrow_condition;
  current_weather.humidity         = w->humidity;
  current_weather.air_quality      = new_aqi > 255 ? 255 : new_aqi;
  current_weather.uv_index         = w->uv_index;

  Serial.printf("[MAIN] Weather: %.1fC RH=%d%% AQI=%d UV=%d tmr=%.0f/%.0fC\n",
                new_temp, w->humidity, new_aqi, w->uv_index, new_tmr_max, new_tmr_min);
}

// Advance time display using millisecond-precise server sync.
// Called every loop iteration; updates the label only when the second changes.
void tick_local_time() {
  if (!time_synced_from_server) return;

  uint64_t local_ms = get_server_local_ms();
  TimeData t;
  ms_to_datetime(local_ms, &t);

  // Only update if the second changed (avoids unnecessary LVGL redraws)
  if (t.second != current_time.second || t.minute != current_time.minute || t.hour != current_time.hour) {
    current_time.hour        = t.hour;
    current_time.minute      = t.minute;
    current_time.second      = t.second;
    current_time.day         = t.day;
    current_time.month       = t.month;
    current_time.year        = t.year;
    current_time.day_of_week = t.day_of_week;
    update_time_display();
    // Date only changes at midnight
    update_date_display();
  }
}

// Legacy dummy time — only used before first server TIME sync
void update_dummy_time() {
  if (time_synced_from_server) {
    tick_local_time();
    return;
  }
  static uint32_t last_update = 0;
  uint32_t now = millis();
  if (now - last_update >= 1000) {
    last_update = now;
    uint32_t seconds_since_midnight = (now / 1000) % (24 * 3600);
    current_time.hour = (seconds_since_midnight / 3600) % 24;
    current_time.minute = (seconds_since_midnight / 60) % 60;
    current_time.second = seconds_since_midnight % 60;
    current_time.day = 24;
    current_time.month = 10;
    current_time.year = 2026;
    current_time.day_of_week = 4;
    update_time_display();
  }
}

// Format date on a single line: "vendredi 24 octobre 2026"
void format_date_single_line(char* buffer, size_t size) {
  snprintf(buffer, size, "%s %d %s %d",
           french_days[current_time.day_of_week],
           current_time.day,
           french_months[current_time.month - 1],
           current_time.year);
}

// Get air quality text in French
const char* get_air_quality_text(uint8_t level) {
  if (level <= 50) return "Bon";
  if (level <= 100) return "Moyen";
  if (level <= 150) return "Mauvais";
  if (level <= 200) return "Très Mauvais";
  return "Dangereux";
}

// Create a semi-transparent white rounded panel (simple, no shadow)
lv_obj_t* create_panel(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, w, h);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(panel, 166, 0); // 65% opaque white (166/255)
  lv_obj_set_style_radius(panel, 0, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

// Disable scrolling on an object and all its children recursively
static void make_fixed(lv_obj_t *obj) {
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
  uint32_t count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < count; i++) {
    make_fixed(lv_obj_get_child(obj, i));
  }
}

// Create the Time+Weather page with fixed positions and larger fonts
void create_time_weather_page() {
  g_page1 = lv_obj_create(nullptr);  // new screen
  lv_obj_set_style_bg_opa(g_page1, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_page1, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *page = g_page1;
  
  // Set background image (full screen)
  lv_obj_t *bg_img = lv_image_create(page);
  lv_image_set_src(bg_img, &background);
  lv_obj_set_pos(bg_img, 0, 0);
  
  // ==================== INFO PANEL (behind everything, full screen) ====================
  create_panel(page, 0, 0, 480, 480);
  
  // ==================== TOP SECTION (Date + Time) ====================
  
  // Date - Top-left: "vendredi 24 octobre 2026" on single line
  date_label = lv_label_create(page);
  lv_obj_set_style_text_font(date_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(date_label, lv_color_hex(0x424242), 0);
  char date_str[40];
  format_date_single_line(date_str, sizeof(date_str));
  lv_label_set_text(date_label, date_str);
  lv_obj_set_pos(date_label, 20, 20);
  lv_obj_set_width(date_label, 440);
  
  // Time - Below date: "14:30" in 96px font
  time_label = lv_label_create(page);
  lv_obj_set_style_text_font(time_label, &montserrat_96, 0);
  lv_obj_set_style_text_color(time_label, lv_color_hex(0x212121), 0);
  lv_label_set_text(time_label, "14:30");
  lv_obj_set_pos(time_label, 20, 60);
  lv_obj_set_width(time_label, 440);
  
  // ==================== LEFT COLUMN (Current Weather Block) ====================
  // Weather block starts below the time (96px font ~114px tall, starting at y=60)
  
  // Current weather icon - 96px, centered in left half
  weather_icon = lv_image_create(page);
  lv_image_set_src(weather_icon, &partly_cloudy_96); // Dummy: partly cloudy
  lv_obj_set_pos(weather_icon, 52, 190);

  // Temperature text - centered relative to weather icon
  temp_label = lv_label_create(page);
  lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(temp_label, lv_color_hex(0x212121), 0);
  lv_obj_set_style_text_align(temp_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(temp_label, "--°C");
  lv_obj_set_pos(temp_label, 52, 298);
  lv_obj_set_width(temp_label, 96);
  
  // Metrics row (compact): RH, Air Quality (score), UV - each with 32px icon above text
  // Text is center-aligned relative to each icon's horizontal center
  // Label width 48px, positioned so label center = icon center (icon_x + 16)
  // RH (humidity) - droplet icon at x=20, center at 36, label x = 36 - 24 = 12
  lv_obj_t *rh_icon = lv_image_create(page);
  lv_image_set_src(rh_icon, &droplet_32);
  lv_obj_set_pos(rh_icon, 20, 345);
  
  rh_label = lv_label_create(page);
  lv_obj_set_style_text_font(rh_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(rh_label, lv_color_hex(0x616161), 0);
  lv_obj_set_style_text_align(rh_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(rh_label, "--%");
  lv_obj_set_pos(rh_label, 12, 382);
  lv_obj_set_width(rh_label, 48);
  
  // Air Quality
  lv_obj_t *aq_icon = lv_image_create(page);
  lv_image_set_src(aq_icon, &air_quality_32);
  lv_obj_set_pos(aq_icon, 90, 345);
  
  aq_label = lv_label_create(page);
  lv_obj_set_style_text_font(aq_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(aq_label, lv_color_hex(0x616161), 0);
  lv_obj_set_style_text_align(aq_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(aq_label, "--");
  lv_obj_set_pos(aq_label, 82, 382);
  lv_obj_set_width(aq_label, 48);
  
  // UV Index
  lv_obj_t *uv_icon = lv_image_create(page);
  lv_image_set_src(uv_icon, &uv_32);
  lv_obj_set_pos(uv_icon, 160, 345);
  
  uv_label = lv_label_create(page);
  lv_obj_set_style_text_font(uv_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(uv_label, lv_color_hex(0x616161), 0);
  lv_obj_set_style_text_align(uv_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(uv_label, "-");
  lv_obj_set_pos(uv_label, 152, 382);
  lv_obj_set_width(uv_label, 48);
  
  // ==================== RIGHT COLUMN (Tomorrow Forecast) ====================
  
  tomorrow_icon = lv_image_create(page);
  lv_image_set_src(tomorrow_icon, &sun_96); // Dummy: sunny tomorrow
  lv_obj_set_pos(tomorrow_icon, 330, 190);
  
  tomorrow_temp = lv_label_create(page);
  lv_obj_set_style_text_font(tomorrow_temp, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(tomorrow_temp, lv_color_hex(0x212121), 0);
  lv_label_set_text(tomorrow_temp, "--/--°C");
  lv_obj_set_pos(tomorrow_temp, 330, 300);
  
  // "Demain" label
  lv_obj_t *demain_label = lv_label_create(page);
  lv_obj_set_style_text_font(demain_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(demain_label, lv_color_hex(0x616161), 0);
  lv_label_set_text(demain_label, "Demain");
  lv_obj_set_pos(demain_label, 330, 340);
}

// Globals for title scroll animation — one set per page
lv_obj_t *g_title_label[2] = {nullptr, nullptr};
lv_obj_t *g_timer_label[2] = {nullptr, nullptr};
lv_obj_t *g_play_icon[2] = {nullptr, nullptr};
int32_t g_title_scroll_dist = 0;
#define TITLE_SCROLL_SPEED_PX_S  60
#define TITLE_PAUSE_MS           2000

// =============================================================================
// Page management (double-tap to switch)
// =============================================================================

static lv_obj_t *g_page2 = nullptr;   // system monitor page
static int g_current_page = 0;

// Double-tap detection
static uint32_t g_last_tap_ms = 0;
static bool g_tap_pending = false;
#define DOUBLE_TAP_WINDOW_MS 350

static void switch_page();
static void on_tap_detected() {
  uint32_t now = millis();
  if (g_tap_pending && (now - g_last_tap_ms) <= DOUBLE_TAP_WINDOW_MS) {
    g_tap_pending = false;
    switch_page();
  } else {
    g_tap_pending = true;
    g_last_tap_ms = now;
  }
}

static void switch_page() {
  g_current_page = 1 - g_current_page;
  lv_obj_t *show = g_current_page == 0 ? g_page1 : g_page2;
  lv_screen_load(show);
  Serial.printf("[UI] Switched to page %d\n", g_current_page + 1);
}

// =============================================================================
// Page 2: System monitor (numeric metrics, no curves)
// =============================================================================

#define SYS_UPDATE_MS 5000

// Metric definitions: label, unit, and current dummy value
struct SysMetric {
  const char *label;
  const char *unit;
  float value;
  float target;
  int decimals;      // 0 or 1
  float scale;       // display multiplier (e.g. 0.001 for MB→GB)
  const char *fmt;   // printf format for value
};

// Metrics shown on page 2 (2 columns x 4 rows)
// Order: CPU%, CPU°C, GPU%, GPU°C, RAM(used/total), Disk I/O%, Net↓, Net↑
enum {
  M_CPU_USAGE, M_CPU_TEMP, M_GPU_USAGE, M_GPU_TEMP,
  M_RAM, M_DISK_IO, M_NET_DOWN, M_NET_UP, M_COUNT
};

static SysMetric g_metrics[M_COUNT] = {
  {"CPU",       "%",    30.0f, 30.0f, 0, 1.0f,   "%.0f"},
  {"CPU Temp",  "°C",   45.0f, 45.0f, 0, 1.0f,   "%.0f"},
  {"GPU",       "%",    10.0f, 10.0f, 0, 1.0f,   "%.0f"},
  {"GPU Temp",  "°C",   50.0f, 50.0f, 0, 1.0f,   "%.0f"},
  {"RAM",       "GB",   16.0f, 48.0f, 0, 1.0f,   "%.1f/%.1f"},  // special: used/total
  {"Disk I/O",  "%",    15.0f, 15.0f, 0, 1.0f,   "%.0f"},
  {"Net Down",  "Mbps", 500.0f,500.0f,1, 1.0f,   "%.1f"},
  {"Net Up",    "Mbps", 50.0f, 50.0f, 1, 1.0f,   "%.1f"},
};

static lv_obj_t *g_sys_time_label = nullptr;
static lv_obj_t *g_metric_value_labels[M_COUNT] = {nullptr};

static void update_dummy_system() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < SYS_UPDATE_MS) return;
  last = now;

  for (int i = 0; i < M_COUNT; i++) {
    // Random walk toward new random target
    if (random(100) < 25) {
      float range;
      switch (i) {
        case M_CPU_USAGE: case M_GPU_USAGE: range = 100.0f; break;
        case M_CPU_TEMP: range = 85.0f; break;
        case M_GPU_TEMP: range = 90.0f; break;
        case M_RAM:      range = 48.0f; break;  // GB
        case M_DISK_IO:  range = 100.0f; break;
        case M_NET_DOWN: range = 1000.0f; break;
        case M_NET_UP:   range = 100.0f; break;
        default: range = 100.0f;
      }
      g_metrics[i].target = random(0, (long)range);
    }
    g_metrics[i].value += (g_metrics[i].target - g_metrics[i].value) * 0.3f;

    // Update label if it exists
    if (g_metric_value_labels[i]) {
      char buf[32];
      if (i == M_RAM) {
        // RAM: show used/total in GB
        snprintf(buf, sizeof(buf), "%.1f/%.1f GB", g_metrics[i].value, 48.0f);
      } else {
        snprintf(buf, sizeof(buf), g_metrics[i].fmt, g_metrics[i].value * g_metrics[i].scale);
        char full[40];
        snprintf(full, sizeof(full), "%s %s", buf, g_metrics[i].unit);
        strncpy(buf, full, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
      }
      lv_label_set_text(g_metric_value_labels[i], buf);
    }
  }

  // Update time label on page 2
  if (g_sys_time_label) {
    char buf[10];
    snprintf(buf, sizeof(buf), "%02d:%02d", current_time.hour, current_time.minute);
    lv_label_set_text(g_sys_time_label, buf);
  }
}

// Create one metric card: label on top, big value below
static lv_obj_t *create_metric_card(lv_obj_t *parent, int col, int row, int idx) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(card, 166, 0);  // 65% opaque — lets background show through
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1,
                       LV_GRID_ALIGN_STRETCH, row, 1);

  // Metric name (small grey)
  lv_obj_t *name = lv_label_create(card);
  lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(name, lv_color_hex(0x757575), 0);
  lv_label_set_text(name, g_metrics[idx].label);
  lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 0);

  // Big value
  lv_obj_t *val = lv_label_create(card);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(val, lv_color_hex(0x212121), 0);
  lv_label_set_text(val, "--");
  lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  g_metric_value_labels[idx] = val;

  return card;
}

// Create page 2: system monitor with numeric metric grid (2 cols x 4 rows)
static void create_system_page() {
  g_page2 = lv_obj_create(nullptr);
  lv_obj_set_style_bg_opa(g_page2, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_page2, LV_OBJ_FLAG_SCROLLABLE);
  // No flex layout on the page itself — absolute positioning like page 1

  // Background image (same as page 1) — at the very back
  lv_obj_t *bg = lv_image_create(g_page2);
  lv_image_set_src(bg, &background);
  lv_obj_set_pos(bg, 0, 0);
  lv_obj_set_size(bg, 480, 480);

  // Semi-transparent white panel over the background (like page 1)
  create_panel(g_page2, 0, 0, 480, 480);

  // Time at top (smaller font than page 1) — absolute position
  g_sys_time_label = lv_label_create(g_page2);
  lv_obj_set_style_text_font(g_sys_time_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(g_sys_time_label, lv_color_hex(0x212121), 0);
  lv_label_set_text(g_sys_time_label, "--:--");
  lv_obj_set_pos(g_sys_time_label, 20, 12);

  // Grid container: 2 columns x 4 rows (leaves room for media bar at bottom)
  lv_obj_t *grid = lv_obj_create(g_page2);
  lv_obj_set_pos(grid, 8, 52);
  lv_obj_set_size(grid, 464, 388);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_gap(grid, 6, 0);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);

  static int32_t col_dsc[] = {229, 229, LV_GRID_TEMPLATE_LAST};
  static int32_t row_dsc[] = {91, 91, 91, 91, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_style_grid_column_dsc_array(grid, col_dsc, 0);
  lv_obj_set_style_grid_row_dsc_array(grid, row_dsc, 0);

  for (int i = 0; i < M_COUNT; i++) {
    create_metric_card(grid, i % 2, i / 2, i);
  }
}

// =============================================================================

// Globals for media player state (server-driven)
static uint64_t media_anchor_server_ms = 0;
static uint32_t media_anchor_local_ms  = 0;
static uint32_t media_position_ms      = 0;
static uint32_t media_duration_ms      = 0;
static uint8_t  media_state            = MEDIA_STOPPED;
static char     media_title[256]       = "";
static char     media_artist[256]      = "";
static bool     media_has_data         = false;

// Server-local ms clock (same anchor as time sync)
static uint64_t server_local_ms() {
  if (!time_synced_from_server) return 0;
  return server_ts_ms + (uint64_t)(millis() - local_rx_millis)
         + (uint64_t)server_tz_min * 60ULL * 1000ULL;
}

// Compute current playback position in ms using the media anchor
static uint32_t current_position_ms() {
  if (media_state != MEDIA_PLAYING || !media_has_data) return media_position_ms;
  uint64_t srv_now = server_local_ms();
  if (srv_now == 0) return media_position_ms;
  uint64_t elapsed = srv_now - media_anchor_server_ms;
  uint64_t pos = (uint64_t)media_position_ms + elapsed;
  if (media_duration_ms > 0 && pos > media_duration_ms) pos = media_duration_ms;
  return (uint32_t)pos;
}

// Start the scroll animation at fixed speed, then handle end-pause and loop
void start_title_scroll() {
  if (g_title_label[g_current_page] == nullptr || g_title_scroll_dist <= 0) return;
  
  // Fixed speed: duration = distance / speed
  int32_t duration_ms = (g_title_scroll_dist * 1000) / TITLE_SCROLL_SPEED_PX_S;
  if (duration_ms < 1000) duration_ms = 1000; // minimum 1s
  
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, g_title_label[g_current_page]);
  lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
    lv_obj_set_x((lv_obj_t*)var, v);
  });
  lv_anim_set_values(&a, 0, -g_title_scroll_dist);
  lv_anim_set_duration(&a, duration_ms); // Fixed speed, variable duration
  lv_anim_set_delay(&a, 0);
  lv_anim_set_playback_duration(&a, 0);
  lv_anim_set_repeat_count(&a, 1);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_set_completed_cb(&a, [](lv_anim_t* anim) {
    // Scroll finished: text is at the end. Pause 2s showing the end.
    lv_timer_create([](lv_timer_t* t) {
      lv_timer_delete(t);
      // Pop back to start instantly
      lv_obj_set_x(g_title_label[g_current_page], 0);
      // Pause 2s at start, then scroll again
      lv_timer_create([](lv_timer_t* t2) {
        lv_timer_delete(t2);
        start_title_scroll();
      }, TITLE_PAUSE_MS, nullptr);
    }, TITLE_PAUSE_MS, nullptr);
  });
  lv_anim_start(&a);
}

// Initial start with 2s delay at the very beginning
void start_title_scroll_initial() {
  lv_timer_create([](lv_timer_t* t) {
    lv_timer_delete(t);
    start_title_scroll();
  }, TITLE_PAUSE_MS, nullptr);
}

// Called by SerialHandler when a MEDIA message arrives from the server.
// Only updates LVGL objects when values actually changed (runs every 1s!).
void on_server_media(const MediaMessageBinary *hdr, const char *title, const char *artist) {
  // Anchor the timer
  media_anchor_server_ms = hdr->timestamp_ms + (uint64_t)server_tz_min * 60ULL * 1000ULL;
  media_anchor_local_ms  = millis();
  media_position_ms      = hdr->position_ms;
  media_duration_ms      = hdr->duration_ms;
  media_has_data         = true;

  // Copy title/artist
  size_t tl = hdr->title_len < 255 ? hdr->title_len : 255;
  size_t al = hdr->artist_len < 255 ? hdr->artist_len : 255;
  char new_title[256], new_artist[256];
  memcpy(new_title, title, tl);  new_title[tl] = '\0';
  memcpy(new_artist, artist, al); new_artist[al] = '\0';

  // Only update title + scroll if track changed
  bool track_changed = (strcmp(new_title, media_title) != 0 || strcmp(new_artist, media_artist) != 0);
  bool state_changed = (media_state != hdr->state);

  if (track_changed) {
    memcpy(media_title, new_title, tl + 1);
    memcpy(media_artist, new_artist, al + 1);

    char display[512];
    if (hdr->state == MEDIA_STOPPED || (tl == 0 && al == 0)) {
      snprintf(display, sizeof(display), "\xE2\x80\x94"); // em dash
    } else if (al > 0) {
      snprintf(display, sizeof(display), "%s \xE2\x80\x94 %s", media_title, media_artist);
    } else {
      snprintf(display, sizeof(display), "%s", media_title);
    }

    // Update title on BOTH pages
    for (int p = 0; p < 2; p++) {
      if (g_title_label[p]) {
        lv_label_set_text(g_title_label[p], display);
        lv_obj_update_layout(g_title_label[p]);
        if (p == g_current_page) {
          int32_t text_width = lv_obj_get_content_width(g_title_label[p]);
          g_title_scroll_dist = text_width - 320;
          if (g_title_scroll_dist < 0) g_title_scroll_dist = 0;
          lv_obj_set_x(g_title_label[p], 0);
          start_title_scroll_initial();
        }
      }
    }
  }

  // Update icon on BOTH pages
  if (state_changed) {
    for (int p = 0; p < 2; p++) {
      if (g_play_icon[p]) {
        lv_image_set_src(g_play_icon[p], hdr->state == MEDIA_PLAYING ? &play_16 : &pause_16);
      }
    }
  }

  media_state = hdr->state;

  if (track_changed || state_changed) {
    Serial.printf("[MAIN] MEDIA: state=%d pos=%ums/%ums \"%s\" — \"%s\"\n",
                  hdr->state, hdr->position_ms, hdr->duration_ms, media_title, media_artist);
  }
  // Timer label is updated cheaply in update_media() every loop
}

// Update media timer display (called every loop, but only writes when second changes)
void update_media() {
  if (!media_has_data) return;

  uint32_t pos = current_position_ms();
  uint32_t dur = media_duration_ms;

  static char last_timer[20] = "";
  char timer_str[20];
  if (media_state == MEDIA_STOPPED || dur == 0) {
    snprintf(timer_str, sizeof(timer_str), "-:--/-:--");
  } else {
    int pos_m = (pos / 1000) / 60;
    int pos_s = (pos / 1000) % 60;
    int tot_m = (dur / 1000) / 60;
    int tot_s = (dur / 1000) % 60;
    snprintf(timer_str, sizeof(timer_str), "%d:%02d/%d:%02d", pos_m, pos_s, tot_m, tot_s);
  }
  if (strcmp(timer_str, last_timer) != 0) {
    // Update timer on BOTH pages
    for (int p = 0; p < 2; p++) {
      if (g_timer_label[p]) lv_label_set_text(g_timer_label[p], timer_str);
    }
    strncpy(last_timer, timer_str, sizeof(last_timer) - 1);
    last_timer[sizeof(last_timer) - 1] = '\0';
  }
}

// Create the media player bar at the bottom of a page
void create_media_player(lv_obj_t *page, int page_idx) {
  
  // Media player container - hard rectangle 480x50 at bottom, non-scrollable, full width
  // Position: x=0, y=480-50=430
  lv_obj_t *media_bar = lv_obj_create(page);
  lv_obj_set_size(media_bar, 480, 32);
  lv_obj_set_pos(media_bar, 0, 448);  // flush with bottom: 448+32 = 480
  lv_obj_set_style_bg_color(media_bar, lv_color_hex(0xFFFFFF), 0); // White
  lv_obj_set_style_bg_opa(media_bar, LV_OPA_50, 0); // 50% transparent (lighter than main panel)
  lv_obj_set_style_border_width(media_bar, 0, 0);
  lv_obj_set_style_radius(media_bar, 0, 0); // Hard corners
  lv_obj_set_style_pad_all(media_bar, 0, 0);
  lv_obj_clear_flag(media_bar, LV_OBJ_FLAG_SCROLLABLE); // Static, not scrollable
  
  // Song title — Author: left-aligned, custom scroll animation
  // Timer + play icon on right; visible title area = 480 - 6 (left) - 130 (timer) - 24 (icon+gap) = 320
  lv_obj_t *title_clip = lv_obj_create(media_bar);
  lv_obj_set_size(title_clip, 320, 22);
  lv_obj_set_pos(title_clip, 6, 2);
  lv_obj_set_style_bg_opa(title_clip, LV_OPA_TRANSP, 0); // Fully transparent, no white rect
  lv_obj_set_style_border_width(title_clip, 0, 0);
  lv_obj_set_style_radius(title_clip, 0, 0);
  lv_obj_set_style_pad_all(title_clip, 0, 0);
  lv_obj_clear_flag(title_clip, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *title_label = lv_label_create(title_clip);
  lv_obj_set_style_text_font(title_label, &g_title_font, 0); // Montserrat 18 + CJK fallback
  lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0); // Black
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(title_label, "\xE2\x80\x94"); // em dash placeholder until server sends media
  lv_obj_set_pos(title_label, 0, 0);
  lv_obj_set_width(title_label, LV_SIZE_CONTENT);
  lv_obj_set_height(title_label, 22);
  
  // Custom scroll animation
  lv_obj_update_layout(title_label);
  int32_t text_width = lv_obj_get_content_width(title_label);
  int32_t scroll_dist = text_width - 320;
  if (scroll_dist < 0) scroll_dist = 0;
  
  g_title_label[page_idx] = title_label;
  g_title_scroll_dist = scroll_dist;
  start_title_scroll_initial();
  
  // Timer - top right, right-aligned: "0:02:34/0:04:56"
  lv_obj_t *timer_label = lv_label_create(media_bar);
  lv_obj_set_style_text_font(timer_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(timer_label, lv_color_hex(0x000000), 0); // Black
  lv_obj_set_style_text_align(timer_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(timer_label, "-:--/-:--");
  lv_obj_set_pos(timer_label, 320, 4);
  lv_obj_set_width(timer_label, 130);
  lv_obj_set_height(timer_label, 22);
  
  // Play/pause icon - to the right of the timer (top row)
  lv_obj_t *play_icon = lv_image_create(media_bar);
  lv_image_set_src(play_icon, &play_16);
  lv_obj_set_pos(play_icon, 456, 5);
  g_play_icon[page_idx] = play_icon;
  
  g_timer_label[page_idx] = timer_label;
  // g_media_last_update removed — timer is server-anchored now
}

// Initialize dummy weather data.
// Comparison fields use sentinel values (0xFF / -999) so the first real
// WEATHER message always triggers a UI update regardless of actual values.
void init_dummy_weather() {
  current_weather.current_temp = -999.0f;
  current_weather.tomorrow_min = -999.0f;
  current_weather.tomorrow_max = -999.0f;
  current_weather.condition = "";
  current_weather.humidity = 0xFF;
  current_weather.air_quality = 0xFF;
  current_weather.uv_index = 0xFF;
  current_weather.condition_id = 0xFF;
  current_weather.tomorrow_condition = 0xFF;
}

// Initialize dummy time
void init_dummy_time() {
  // Start with a reasonable time
  current_time.hour = 14;
  current_time.minute = 30;
  current_time.second = 0;
  current_time.day = 24;
  current_time.month = 10; // October
  current_time.year = 2026;
  current_time.day_of_week = 4; // Friday (Jeudi)
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Setup...");
  touch_init();

  // Initialize dummy data
  init_dummy_time();
  init_dummy_weather();

  // Initialize serial handler (uses UART0 / Serial, same port as debug)
  // Note: on ESP32 UART0 is shared with USB serial — the server connects via USB
  g_serial.on_time(on_server_time);
  g_serial.on_weather(on_server_weather);
  g_serial.on_media(on_server_media);
  g_serial.on_conn_state(on_conn_state);
  g_serial.begin(115200);

  // Initialize 9-bit mode SPI
  bus = new Arduino_ESP32SPI(
    GFX_NOT_DEFINED /* DC */, 39 /* CS */, 48 /* SCK */, 47 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

  // Initialize RGB panel (hardware specific)
  Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
#if 0
    4 /* R0 */, 5 /* R1 */, 6 /* R2 */, 7 /* R3 */, 15 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    11 /* B0 */, 12 /* B1 */, 13 /* B2 */, 14 /* B3 */, 0 /* B4 */,
#else
    11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */,
#endif
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

  // Initialize display panel
  gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_4848s040_init_operations, sizeof(st7701_4848s040_init_operations));

  // Check if display initialization succeeded
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }

  // Turn on backlight
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  analogWrite(GFX_BL, TFT_BRIGHTNESS);
#endif

  // Display startup screen
  gfx->setCursor(100, 200);
  gfx->displayOn();
  gfx->fillScreen(BLACK);
  gfx->setTextColor(BLUE);
  gfx->setTextSize(6 /* x scale */, 6 /* y scale */, 2 /* pixel_margin */);
  gfx->println("Starting...");
  gfx->setRotation((4 - TFT_ROTATION) % 4); // Adjust rotation for Arduino_GFX

  // Initialize LVGL
  lv_init();

  // Set tick source for LVGL timing
  lv_tick_set_cb(my_tick);

  // Create LVGL display
  lv_display_t *disp;
  disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_rotation(disp, TFT_ROTATION);

  // Initialize touch input (simplified, no edge detection)
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  // Setup title font with fallback chain (Montserrat 18 -> fallback CJK font)
  g_title_font = lv_font_montserrat_18;
  g_title_font.fallback = &song_title_fallback_18;
  
  // Build GUI - Page 1 (time+weather) and Page 2 (system monitor)
  create_time_weather_page();
  create_media_player(g_page1, 0);   // media bar on page 1
  create_system_page();
  create_media_player(g_page2, 1);   // media bar on page 2 (same layout)

  // Load page 1 as the active screen
  lv_screen_load(g_page1);

  // Lock everything: disable scrolling on all screens and children
  make_fixed(g_page1);
  make_fixed(g_page2);

  Serial.println("Time+Weather page created. Ready!");
}

void loop()
{
  // Process incoming serial frames (heartbeat, time, etc.)
  g_serial.process();

  // Update time display (ticks locally when synced, dummy otherwise)
  update_dummy_time();
  
  // Update media progress
  update_media();

  // Update page 2 dummy system charts
  update_dummy_system();
  
  // Let LVGL handle GUI tasks
  lv_timer_handler();
  delay(5);
}
