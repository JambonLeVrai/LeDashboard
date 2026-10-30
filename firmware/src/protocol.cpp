#include "protocol.h"
#include "data/weather_data.h"
#include <string.h>

// French day names
const char* DAY_NAMES_FR[7] = {
    "Lundi",
    "Mardi",
    "Mercredi",
    "Jeudi",
    "Vendredi",
    "Samedi",
    "Dimanche"
};

// French month names
const char* MONTH_NAMES_FR[12] = {
    "janvier",
    "février",
    "mars",
    "avril",
    "mai",
    "juin",
    "juillet",
    "août",
    "septembre",
    "octobre",
    "novembre",
    "décembre"
};

// Air Quality Index thresholds and strings
const char* get_air_quality_string(uint8_t aqi) {
    // AQI scale: 0-50 Good, 51-100 Moderate, 101-150 Unhealthy, 151-200 Very Unhealthy, 201+ Hazardous
    if (aqi <= 50) return AQ_GOOD;
    else if (aqi <= 100) return AQ_MODERATE;
    else if (aqi <= 150) return AQ_UNHEALTHY;
    else if (aqi <= 200) return AQ_VERY_UNHEALTHY;
    else return AQ_HAZARDOUS;
}

// UV Index thresholds and strings
const char* get_uv_index_string(uint8_t uv) {
    // UV Index: 0-2 Low, 3-5 Moderate, 6-7 High, 8-10 Very High, 11+ Extreme
    if (uv <= 2) return UV_LOW;
    else if (uv <= 5) return UV_MODERATE;
    else if (uv <= 7) return UV_HIGH;
    else if (uv <= 10) return UV_VERY_HIGH;
    else return UV_EXTREME;
}

// CRC-16/CCITT implementation
uint16_t crc16_ccitt(const uint8_t *buf, unsigned int len) {
    uint16_t crc = 0x0000;
    for (unsigned int i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// =============================================================================
// Frame Encoding
// =============================================================================

// Apply byte stuffing to a body segment in-place into out buffer.
// Returns number of bytes written.
static unsigned int stuff_bytes(const uint8_t *src, unsigned int src_len, uint8_t *dst) {
    unsigned int n = 0;
    for (unsigned int i = 0; i < src_len; i++) {
        uint8_t b = src[i];
        if (b == SOF || b == EOF || b == ESC) {
            dst[n++] = ESC;
            dst[n++] = b ^ 0xA5;
        } else {
            dst[n++] = b;
        }
    }
    return n;
}

unsigned int encode_frame(uint8_t msg_type, const uint8_t *payload, unsigned int payload_len,
                          uint8_t *out_buf, unsigned int out_cap) {
    if (payload_len > MAX_PAYLOAD_SIZE) return 0;
    // Worst case: every byte stuffed = 2x body + SOF + EOF
    unsigned int body_len = 3 + payload_len + 2; // type + length(2) + payload + crc(2)
    unsigned int needed = 1 + body_len * 2 + 1;  // SOF + stuffed body + EOF
    if (out_cap < needed) return 0;

    unsigned int pos = 0;
    out_buf[pos++] = SOF;

    // Build unstuffed body: type + length(BE) + payload + crc(BE)
    uint8_t body[MAX_PAYLOAD_SIZE + 5];
    body[0] = msg_type;
    body[1] = (payload_len >> 8) & 0xFF;
    body[2] = payload_len & 0xFF;
    if (payload_len > 0 && payload != NULL) {
        memcpy(&body[3], payload, payload_len);
    }
    // CRC over type + length + payload
    uint16_t crc = crc16_ccitt(body, 3 + payload_len);
    body[3 + payload_len]     = (crc >> 8) & 0xFF;
    body[3 + payload_len + 1] = crc & 0xFF;

    // Stuff the body into out_buf
    pos += stuff_bytes(body, body_len, &out_buf[pos]);

    out_buf[pos++] = EOF;
    return pos;
}

// =============================================================================
// Frame Decoding (incremental state machine)
// =============================================================================

void decoder_init(frame_decoder_t *dec, frame_callback_t on_frame, error_callback_t on_error) {
    memset(dec, 0, sizeof(*dec));
    dec->state = DEC_WAIT_SOF;
    dec->on_frame = on_frame;
    dec->on_error = on_error;
}

void decoder_reset(frame_decoder_t *dec) {
    dec->state = DEC_WAIT_SOF;
    dec->buf_len = 0;
    dec->payload_len = 0;
    dec->esc_flag = 0;
}

// Complete a frame from the accumulated buffer; returns 1 if a frame was emitted.
static unsigned int decoder_finish(frame_decoder_t *dec) {
    if (dec->buf_len < 5) { // need at least type + length(2) + crc(2)
        decoder_reset(dec);
        return 0;
    }
    uint8_t msg_type = dec->buf[0];
    unsigned int plen = ((unsigned int)dec->buf[1] << 8) | dec->buf[2];
    if (plen > MAX_PAYLOAD_SIZE || dec->buf_len < 3 + plen + 2) {
        decoder_reset(dec);
        return 0;
    }
    const uint8_t *payload = &dec->buf[3];
    uint16_t recv_crc = ((uint16_t)dec->buf[3 + plen] << 8) | dec->buf[3 + plen + 1];
    uint16_t calc_crc = crc16_ccitt(dec->buf, 3 + plen);

    unsigned int emitted = 0;
    if (calc_crc == recv_crc) {
        if (dec->on_frame) dec->on_frame(msg_type, payload, plen);
        emitted = 1;
    } else {
        if (dec->on_error) dec->on_error(ERR_CRC_MISMATCH, msg_type);
    }
    decoder_reset(dec);
    return emitted;
}

unsigned int decoder_feed(frame_decoder_t *dec, const uint8_t *data, unsigned int len) {
    unsigned int frames = 0;
    for (unsigned int i = 0; i < len; i++) {
        uint8_t raw = data[i];

        if (dec->state == DEC_WAIT_SOF) {
            if (raw == SOF) {
                decoder_reset(dec);
                dec->state = DEC_READ_BODY;
            }
            continue;
        }

        // Unstuff every byte between SOF and EOF
        uint8_t b;
        if (dec->esc_flag) {
            b = raw ^ 0xA5;
            dec->esc_flag = 0;
        } else if (raw == ESC) {
            dec->esc_flag = 1;
            continue;
        } else if (raw == EOF && dec->state == DEC_EXPECT_EOF) {
            frames += decoder_finish(dec);
            continue;
        } else {
            b = raw;
        }

        if (dec->state == DEC_READ_BODY) {
            if (dec->buf_len >= sizeof(dec->buf)) {
                decoder_reset(dec);
                continue;
            }
            dec->buf[dec->buf_len++] = b;
            // Once we have type + length, check if we've got the full body + crc
            if (dec->buf_len == 3) {
                dec->payload_len = ((unsigned int)dec->buf[1] << 8) | dec->buf[2];
                if (dec->payload_len > MAX_PAYLOAD_SIZE) {
                    decoder_reset(dec);
                    continue;
                }
            }
            if (dec->buf_len == 3 + dec->payload_len + 2) {
                dec->state = DEC_EXPECT_EOF;
            }
        } else {
            // Stray byte between body and EOF: resync
            decoder_reset(dec);
            if (b == SOF) dec->state = DEC_READ_BODY;
        }
    }
    return frames;
}

// Weather icon names based on condition ID
const char* getWeatherIconName(uint8_t condition_id) {
    // Simplified mapping - in real implementation, this would map to actual icon names
    // that correspond to your image assets
    switch(condition_id) {
        case COND_CLEAR_DAY: return "clear_day";
        case COND_CLEAR_NIGHT: return "clear_night";
        case COND_PARTLY_CLOUDY: return "partly_cloudy";
        case COND_CLOUDY: return "cloudy";
        case COND_OVERCAST: return "overcast";
        case COND_RAIN_LIGHT: return "light_rain";
        case COND_RAIN_MODERATE: return "rain";
        case COND_RAIN_HEAVY: return "heavy_rain";
        case COND_THUNDERSTORM: return "thunderstorm";
        case COND_SNOW: return "snow";
        case COND_FOG: return "fog";
        case COND_MIST: return "mist";
        case COND_WINDY: return "windy";
        default: return "unknown";
    }
}
