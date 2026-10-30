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

// Forward declarations
void update_time_display();
void update_date_display();
void format_date_single_line(char* buffer, size_t size);

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
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
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
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

// Create the Time+Weather page with fixed positions and larger fonts
void create_time_weather_page() {
  lv_obj_t *page = lv_screen_active();
  
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
  lv_obj_t *weather_icon = lv_image_create(page);
  lv_image_set_src(weather_icon, &partly_cloudy_96); // Dummy: partly cloudy
  lv_obj_set_pos(weather_icon, 52, 190); // Centered: (240-96)/2 = 72, but left-aligned
  
  // Temperature text - centered relative to weather icon (icon at x=52, 96px wide, center=100)
  lv_obj_t *temp_label = lv_label_create(page);
  lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(temp_label, lv_color_hex(0x212121), 0);
  lv_obj_set_style_text_align(temp_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(temp_label, "22°C");
  lv_obj_set_pos(temp_label, 52, 298); // Same x as icon
  lv_obj_set_width(temp_label, 96);    // Same width as icon for centering
  
  // Metrics row (compact): RH, Air Quality (score), UV - each with 32px icon above text
  // Text is center-aligned relative to each icon's horizontal center
  // Label width 48px, positioned so label center = icon center (icon_x + 16)
  // RH (humidity) - droplet icon at x=20, center at 36, label x = 36 - 24 = 12
  lv_obj_t *rh_icon = lv_image_create(page);
  lv_image_set_src(rh_icon, &droplet_32);
  lv_obj_set_pos(rh_icon, 20, 345);
  
  lv_obj_t *rh_label = lv_label_create(page);
  lv_obj_set_style_text_font(rh_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(rh_label, lv_color_hex(0x616161), 0);
  lv_obj_set_style_text_align(rh_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(rh_label, "65%");
  lv_obj_set_pos(rh_label, 12, 382);
  lv_obj_set_width(rh_label, 48);
  
  // Air Quality - score 0-100, icon at x=90, center at 106, label x = 106 - 24 = 82
  lv_obj_t *aq_icon = lv_image_create(page);
  lv_image_set_src(aq_icon, &air_quality_32);
  lv_obj_set_pos(aq_icon, 90, 345);
  
  lv_obj_t *aq_label = lv_label_create(page);
  lv_obj_set_style_text_font(aq_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(aq_label, lv_color_hex(0x616161), 0);
  lv_obj_set_style_text_align(aq_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(aq_label, "85");
  lv_obj_set_pos(aq_label, 82, 382);
  lv_obj_set_width(aq_label, 48);
  
  // UV Index - icon at x=160, center at 176, label x = 176 - 24 = 152
  lv_obj_t *uv_icon = lv_image_create(page);
  lv_image_set_src(uv_icon, &uv_32);
  lv_obj_set_pos(uv_icon, 160, 345);
  
  lv_obj_t *uv_label = lv_label_create(page);
  lv_obj_set_style_text_font(uv_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(uv_label, lv_color_hex(0x616161), 0);
  lv_obj_set_style_text_align(uv_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(uv_label, "3");
  lv_obj_set_pos(uv_label, 152, 382);
  lv_obj_set_width(uv_label, 48);
  
  // ==================== RIGHT COLUMN (Tomorrow Forecast) ====================
  
  // Tomorrow weather icon - 96px, right side
  lv_obj_t *tomorrow_icon = lv_image_create(page);
  lv_image_set_src(tomorrow_icon, &sun_96); // Dummy: sunny tomorrow
  lv_obj_set_pos(tomorrow_icon, 330, 190);
  
  // Tomorrow temperature range
  lv_obj_t *tomorrow_temp = lv_label_create(page);
  lv_obj_set_style_text_font(tomorrow_temp, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(tomorrow_temp, lv_color_hex(0x212121), 0);
  lv_label_set_text(tomorrow_temp, "15/8°C");
  lv_obj_set_pos(tomorrow_temp, 330, 300);
  
  // "Demain" label
  lv_obj_t *demain_label = lv_label_create(page);
  lv_obj_set_style_text_font(demain_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(demain_label, lv_color_hex(0x616161), 0);
  lv_label_set_text(demain_label, "Demain");
  lv_obj_set_pos(demain_label, 330, 340);
}

// Globals for title scroll animation
lv_obj_t *g_title_label = nullptr;
int32_t g_title_scroll_dist = 0;
#define TITLE_SCROLL_SPEED_PX_S  60   // Fixed scroll speed: 60 pixels per second
#define TITLE_PAUSE_MS           2000 // 2 second pause at start and end

// Globals for media player state
lv_obj_t *g_timer_label = nullptr;
lv_obj_t *g_play_icon = nullptr;
int g_media_position_sec = 0;      // Current position in seconds
const int MEDIA_DURATION_SEC = 120; // 2:00 song
bool g_media_playing = true;
uint32_t g_media_last_update = 0;

// Start the scroll animation at fixed speed, then handle end-pause and loop
void start_title_scroll() {
  if (g_title_label == nullptr || g_title_scroll_dist <= 0) return;
  
  // Fixed speed: duration = distance / speed
  int32_t duration_ms = (g_title_scroll_dist * 1000) / TITLE_SCROLL_SPEED_PX_S;
  if (duration_ms < 1000) duration_ms = 1000; // minimum 1s
  
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, g_title_label);
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
      lv_obj_set_x(g_title_label, 0);
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

// Update media progress (called every loop iteration)
void update_media() {
  if (!g_media_playing) return;
  
  uint32_t now = millis();
  if (now - g_media_last_update >= 1000) {
    g_media_last_update = now;
    g_media_position_sec++;
    if (g_media_position_sec >= MEDIA_DURATION_SEC) {
      g_media_position_sec = 0; // Loop the song
    }
    
    // Update timer text: "M:SS/M:SS" format
    if (g_timer_label != nullptr) {
      char timer_str[20];
      int pos_m = g_media_position_sec / 60;
      int pos_s = g_media_position_sec % 60;
      int tot_m = MEDIA_DURATION_SEC / 60;
      int tot_s = MEDIA_DURATION_SEC % 60;
      snprintf(timer_str, sizeof(timer_str), "%d:%02d/%d:%02d", pos_m, pos_s, tot_m, tot_s);
      lv_label_set_text(g_timer_label, timer_str);
    }
  }
}

// Create the media player bar at the bottom of the screen
void create_media_player() {
  lv_obj_t *page = lv_screen_active();
  
  // Media player container - hard rectangle 480x50 at bottom, non-scrollable, full width
  // Position: x=0, y=480-50=430
  lv_obj_t *media_bar = lv_obj_create(page);
  lv_obj_set_size(media_bar, 480, 32);
  lv_obj_set_pos(media_bar, 0, 454);
  lv_obj_set_style_bg_color(media_bar, lv_color_hex(0xFFFFFF), 0); // White
  lv_obj_set_style_bg_opa(media_bar, LV_OPA_50, 0); // 50% transparent (lighter than main panel)
  lv_obj_set_style_border_width(media_bar, 0, 0);
  lv_obj_set_style_radius(media_bar, 0, 0); // Hard corners
  lv_obj_set_style_pad_all(media_bar, 0, 0);
  lv_obj_remove_flag(media_bar, LV_OBJ_FLAG_SCROLLABLE); // Static, not scrollable
  
  // Song title — Author: left-aligned, custom scroll animation
  // Timer + play icon on right; visible title area = 480 - 6 (left) - 130 (timer) - 24 (icon+gap) = 320
  lv_obj_t *title_clip = lv_obj_create(media_bar);
  lv_obj_set_size(title_clip, 320, 22);
  lv_obj_set_pos(title_clip, 6, 2);
  lv_obj_set_style_bg_opa(title_clip, LV_OPA_TRANSP, 0); // Fully transparent, no white rect
  lv_obj_set_style_border_width(title_clip, 0, 0);
  lv_obj_set_style_radius(title_clip, 0, 0);
  lv_obj_set_style_pad_all(title_clip, 0, 0);
  lv_obj_remove_flag(title_clip, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *title_label = lv_label_create(title_clip);
  lv_obj_set_style_text_font(title_label, &g_title_font, 0); // Montserrat 18 + CJK fallback
  lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0); // Black
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(title_label, "A Very Long Song Title That Definitely Does Not Fit In The Allocated Space — Famous Artist Name feat. Another Artist");
  lv_obj_set_pos(title_label, 0, 0);
  lv_obj_set_width(title_label, LV_SIZE_CONTENT);
  lv_obj_set_height(title_label, 22);
  
  // Custom scroll animation
  lv_obj_update_layout(title_label);
  int32_t text_width = lv_obj_get_content_width(title_label);
  int32_t scroll_dist = text_width - 320;
  if (scroll_dist < 0) scroll_dist = 0;
  
  g_title_label = title_label;
  g_title_scroll_dist = scroll_dist;
  start_title_scroll_initial();
  
  // Timer - top right, right-aligned: "0:02:34/0:04:56"
  lv_obj_t *timer_label = lv_label_create(media_bar);
  lv_obj_set_style_text_font(timer_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(timer_label, lv_color_hex(0x000000), 0); // Black
  lv_obj_set_style_text_align(timer_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(timer_label, "0:02:34/0:04:56");
  lv_obj_set_pos(timer_label, 320, 4);
  lv_obj_set_width(timer_label, 130);
  lv_obj_set_height(timer_label, 22);
  
  // Play/pause icon - to the right of the timer (top row)
  lv_obj_t *play_icon = lv_image_create(media_bar);
  lv_image_set_src(play_icon, &play_16);
  lv_obj_set_pos(play_icon, 456, 5);
  g_play_icon = play_icon;
  
  g_timer_label = timer_label;
  g_media_last_update = millis();
}

// Initialize dummy weather data
void init_dummy_weather() {
  current_weather.current_temp = 22.0f;
  current_weather.tomorrow_min = 8.0f;
  current_weather.tomorrow_max = 15.0f;
  current_weather.condition = "Partiellement nuageux";
  current_weather.humidity = 65;
  current_weather.air_quality = 30; // Good quality
  current_weather.uv_index = 3;
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
  
  // Build GUI - Time+Weather page with fixed positions
  create_time_weather_page();
  create_media_player();
  
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
  
  // Let LVGL handle GUI tasks
  lv_timer_handler();
  delay(5);
}
