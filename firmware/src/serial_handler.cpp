// Serial communication handler for LeDashboard ESP32 firmware.
// Reads frames from UART0, dispatches HEARTBEAT and TIME messages,
// sends ACKs and periodic heartbeats.

#include "serial_handler.h"
#include <Arduino.h>

// Global instance
SerialHandler g_serial;

// Static trampolines — decoder callbacks are plain C function pointers,
// so we forward to the global instance.
void SerialHandler::on_frame_static(uint8_t type, const uint8_t *payload, unsigned int len) {
    g_serial.handle_frame(type, payload, len);
}

void SerialHandler::on_error_static(uint8_t error_code, uint8_t msg_type) {
    g_serial.handle_error(error_code, msg_type);
}

SerialHandler::SerialHandler()
    : state_(CONN_IDLE),
      hb_sequence_(0),
      last_hb_sent_(0),
      last_hb_received_(0),
      time_handler_(nullptr),
      conn_handler_(nullptr) {}

void SerialHandler::begin(unsigned long baud_rate) {
    Serial.begin(baud_rate);
    decoder_init(&decoder_, on_frame_static, on_error_static);
    last_hb_sent_ = millis();
    last_hb_received_ = millis();
    Serial.printf("[SERIAL] Initialized @ %lu baud\n", baud_rate);
}

void SerialHandler::send_frame(uint8_t msg_type, const uint8_t *payload, unsigned int len) {
    // Static buffer — avoid stack overflow (ESP32 loop task stack is ~8KB)
    static uint8_t buf[MAX_PAYLOAD_SIZE * 2 + FRAME_OVERHEAD + 2];
    unsigned int n = encode_frame(msg_type, payload, len, buf, sizeof(buf));
    if (n > 0) {
        Serial.write(buf, n);
    }
}

void SerialHandler::send_ack(uint8_t acked_type) {
    uint8_t payload[2] = { acked_type, 0 }; // acked_type + sequence(0)
    send_frame(MSG_ACK, payload, sizeof(payload));
}

void SerialHandler::send_nack(uint8_t nacked_type, uint8_t error_code) {
    uint8_t payload[3] = { nacked_type, error_code, 0 };
    send_frame(MSG_NACK, payload, sizeof(payload));
}

void SerialHandler::send_heartbeat() {
    HeartbeatMessage hb;
    hb.sequence = hb_sequence_++;
    hb.protocol_major = PROTO_MAJOR;
    hb.protocol_minor = PROTO_MINOR;
    send_frame(MSG_HEARTBEAT, reinterpret_cast<uint8_t *>(&hb), sizeof(hb));
    last_hb_sent_ = millis();
}

void SerialHandler::set_state(connection_state_t s) {
    if (s != state_) {
        state_ = s;
        if (conn_handler_) conn_handler_(s);
        Serial.printf("[SERIAL] Connection state -> %d\n", (int)s);
    }
}

void SerialHandler::handle_frame(uint8_t type, const uint8_t *payload, unsigned int len) {
    switch (type) {
        case MSG_HEARTBEAT: {
            // Respond to heartbeat ping with heartbeat pong
            last_hb_received_ = millis();
            if (state_ != CONN_CONNECTED) set_state(CONN_CONNECTED);
            // Echo heartbeat back
            send_heartbeat();
            Serial.printf("[SERIAL] HEARTBEAT received (seq=%d), responded\n",
                          len > 0 ? payload[0] : -1);
            break;
        }

        case MSG_TIME: {
            if (len >= sizeof(TimeMessageBinary) && time_handler_) {
                TimeMessageBinary t;
                memcpy(&t, payload, sizeof(t));
                time_handler_(&t);
                Serial.printf("[SERIAL] TIME received: %02d:%02d:%02d %02d/%02d/%04d dow=%d\n",
                              t.hour, t.minute, t.second, t.day, t.month, t.year, t.day_of_week);
            }
            send_ack(MSG_TIME);
            break;
        }

        default:
            // ACK any other valid frame for now
            Serial.printf("[SERIAL] Frame type=0x%02X len=%u (unhandled, ACK)\n", type, len);
            send_ack(type);
            break;
    }
}

void SerialHandler::handle_error(uint8_t error_code, uint8_t msg_type) {
    Serial.printf("[SERIAL] CRC error on type=0x%02X -> NACK\n", msg_type);
    send_nack(msg_type, error_code);
}

void SerialHandler::check_connection() {
    uint32_t now = millis();

    // Send periodic heartbeat
    if (now - last_hb_sent_ >= HB_INTERVAL_MS) {
        send_heartbeat();
    }

    // Check for timeout
    if (state_ == CONN_CONNECTED && (now - last_hb_received_ > HB_TIMEOUT_MS)) {
        set_state(CONN_LOST);
    } else if (state_ == CONN_LOST && (now - last_hb_received_ <= HB_TIMEOUT_MS)) {
        set_state(CONN_CONNECTED);
    }
}

void SerialHandler::process() {
    // Read all available bytes and feed to decoder
    while (Serial.available()) {
        uint8_t buf[256];
        int n = Serial.readBytes(buf, sizeof(buf));
        if (n > 0) {
            decoder_feed(&decoder_, buf, (unsigned int)n);
        }
    }

    check_connection();
}
