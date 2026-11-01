#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// Protocol Constants
// =============================================================================

// Message type definitions
#define MSG_HEARTBEAT     0x00
#define MSG_TIME          0x01
#define MSG_WEATHER       0x02
#define MSG_MEDIA         0x03
#define MSG_NOTIFICATION  0x04
#define MSG_PORTFOLIO     0x05
#define MSG_SYSTEM        0x06
#define MSG_CALENDAR      0x07
#define MSG_ACK           0x08
#define MSG_NACK          0x09
#define MSG_ERROR         0x0A
#define MSG_CONFIG        0x0B
#define MSG_REQUEST       0x0C
#define MSG_TOUCH         0x0D
#define MSG_COMMAND       0x0E
#define MSG_DEBUG         0x0F

// =============================================================================
// Data Format Selection (as requested by user)
// =============================================================================

// Format constants - single point of change
#define USE_JSON_FORMAT   0
#define USE_BINARY_FORMAT 1

// Current active format (change this to switch between JSON and binary)
#define DATA_FORMAT USE_BINARY_FORMAT

// Helper macro for conditional compilation
#if DATA_FORMAT == USE_JSON_FORMAT
    #define FORMAT_IS_JSON 1
#else
    #define FORMAT_IS_JSON 0
#endif

// =============================================================================
// Frame Constants
// =============================================================================

#define SOF 0xAA        // Start of Frame
#define EOF 0x55        // End of Frame
#define ESC 0x5A        // Escape character

// =============================================================================
// CRC Calculation
// =============================================================================

uint16_t crc16_ccitt(const uint8_t *buf, unsigned int len);

// =============================================================================
// Frame Encoding / Decoding
// =============================================================================

#define MAX_PAYLOAD_SIZE 4095
#define FRAME_OVERHEAD   6   // SOF + Type + Length(2) + CRC(2) + EOF

// Encode a message into a complete frame (SOF..EOF with byte stuffing).
// Returns the number of bytes written to out_buf (max out_cap).
// out_buf must be at least MAX_PAYLOAD_SIZE*2 + FRAME_OVERHEAD + 2 bytes.
unsigned int encode_frame(uint8_t msg_type, const uint8_t *payload, unsigned int payload_len,
                          uint8_t *out_buf, unsigned int out_cap);

// Incremental frame decoder state machine.
// Feed bytes one chunk at a time; calls on_frame(type, payload, len) for each
// valid frame, and on_error(err_code, type) for CRC failures.
typedef void (*frame_callback_t)(uint8_t type, const uint8_t *payload, unsigned int len);
typedef void (*error_callback_t)(uint8_t error_code, uint8_t msg_type);

typedef enum {
    DEC_WAIT_SOF,
    DEC_READ_BODY,
    DEC_EXPECT_EOF
} decoder_state_t;

typedef struct {
    decoder_state_t state;
    uint8_t  buf[MAX_PAYLOAD_SIZE + 5]; // type + length(2) + payload + crc(2)
    unsigned int buf_len;
    unsigned int payload_len;
    uint8_t  esc_flag;
    frame_callback_t on_frame;
    error_callback_t on_error;
} frame_decoder_t;

void decoder_init(frame_decoder_t *dec, frame_callback_t on_frame, error_callback_t on_error);
void decoder_reset(frame_decoder_t *dec);
// Feed raw bytes; returns number of frames decoded.
unsigned int decoder_feed(frame_decoder_t *dec, const uint8_t *data, unsigned int len);

// Error codes for decoder
#define ERR_CRC_MISMATCH  0x02

// =============================================================================
// Message Structures (Binary Format)
// =============================================================================

// Time message
#pragma pack(push, 1)
typedef struct {
    uint64_t timestamp_ms;    // 8 bytes — Unix timestamp in milliseconds (UTC)
    int16_t  timezone_min;    // 2 bytes — local timezone offset from UTC in minutes
    uint8_t  dst_flag;        // 1 byte  — DST active (0/1)
    uint8_t  hour;            // 1 byte  — pre-computed local time fields
    uint8_t  minute;          // 1 byte
    uint8_t  second;          // 1 byte
    uint8_t  day;             // 1 byte
    uint8_t  month;           // 1 byte
    uint16_t year;            // 2 bytes (little-endian)
    uint8_t  day_of_week;     // 1 byte  — 0=Monday .. 6=Sunday
} TimeMessageBinary;          // total: 18 bytes

// Heartbeat message
typedef struct {
    uint8_t sequence;
    uint8_t protocol_major;
    uint8_t protocol_minor;
} HeartbeatMessage;

// Compact weather message — exactly what the dashboard displays (26 bytes)
typedef struct {
    uint64_t timestamp_ms;       // 8 — UTC ms when data was fetched
    int16_t  temp_x10;           // 2 — current temp * 10
    int16_t  feels_like_x10;     // 2 — feels-like temp * 10
    uint8_t  humidity;           // 1 — 0-100%
    uint8_t  condition_id;       // 1 — mapped to icon (COND_* codes)
    uint8_t  uv_index;           // 1 — 0-11+
    uint16_t air_quality;        // 2 — European AQI (0-500)
    uint16_t wind_speed_x10;     // 2 — m/s * 10
    uint16_t wind_direction;     // 2 — degrees 0-359
    int16_t  tomorrow_min_x10;   // 2 — tomorrow min temp * 10
    int16_t  tomorrow_max_x10;   // 2 — tomorrow max temp * 10
    uint8_t  tomorrow_condition; // 1 — tomorrow condition icon
    uint16_t pressure_x10;       // 2 — hPa * 10
} WeatherMessageBinary;          // total: 28 bytes

// Media message — variable length: header + title + artist strings
#pragma pack(push, 1)
typedef struct {
    uint64_t timestamp_ms;   // 8 — server UTC ms when sent (timer sync)
    uint32_t position_ms;    // 4 — current playback position
    uint32_t duration_ms;    // 4 — total track duration
    uint8_t  state;          // 1 — 0=stopped, 1=paused, 2=playing
    uint8_t  title_len;      // 1
    uint8_t  artist_len;     // 1
    // char title[title_len]; char artist[artist_len];
} MediaMessageBinary;        // header: 19 bytes
#pragma pack(pop)

#define MEDIA_STOPPED 0
#define MEDIA_PAUSED  1
#define MEDIA_PLAYING 2

// Discovery (auto-detect peripheral)
#define MSG_DISCOVER     0x80
#define MSG_DISCOVER_ACK 0x81
extern const char DISCOVER_MAGIC[];
extern const char DISCOVER_ACK_PAYLOAD[];
#pragma pack(pop)

// (Old complex weather structs removed — replaced by compact WeatherMessageBinary above)

// =============================================================================
// French Localization Constants
// =============================================================================

// Days of week (French)
#define DAY_MONDAY    0
#define DAY_TUESDAY   1
#define DAY_WEDNESDAY 2
#define DAY_THURSDAY  3
#define DAY_FRIDAY    4
#define DAY_SATURDAY  5
#define DAY_SUNDAY    6

extern const char* DAY_NAMES_FR[7];

// Months (French)
#define MONTH_JANUARY   1
#define MONTH_FEBRUARY  2
#define MONTH_MARCH     3
#define MONTH_APRIL     4
#define MONTH_MAY       5
#define MONTH_JUNE      6
#define MONTH_JULY      7
#define MONTH_AUGUST    8
#define MONTH_SEPTEMBER 9
#define MONTH_OCTOBER   10
#define MONTH_NOVEMBER  11
#define MONTH_DECEMBER  12

extern const char* MONTH_NAMES_FR[12];

// Air Quality strings (French)
#define AQ_GOOD          "Bon"
#define AQ_MODERATE      "Moyen"
#define AQ_UNHEALTHY    "Mauvais"
#define AQ_VERY_UNHEALTHY "Tres Mauvais"
#define AQ_HAZARDOUS    "Dangereux"

// UV Index strings (French)
#define UV_LOW           "Faible"
#define UV_MODERATE      "Modere"
#define UV_HIGH          "Eleve"
#define UV_VERY_HIGH     "Tres Eleve"
#define UV_EXTREME       "Extreme"

// Get air quality string
const char* get_air_quality_string(uint8_t aqi);

// Get UV index string
const char* get_uv_index_string(uint8_t uv);

// Get weather icon name for a condition ID
const char* getWeatherIconName(uint8_t condition_id);

#endif // PROTOCOL_H
