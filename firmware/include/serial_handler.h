#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H

#include <stdint.h>
#include "protocol.h"

// =============================================================================
// Connection state
// =============================================================================

typedef enum {
    CONN_IDLE = 0,       // No server detected yet
    CONN_CONNECTED,      // Heartbeat exchange active
    CONN_LOST            // Server stopped responding
} connection_state_t;

// =============================================================================
// Callbacks for message handlers (set by main.cpp)
// =============================================================================

// Called when a TIME message is received and parsed.
// time_msg points to a valid TimeMessageBinary.
typedef void (*time_handler_t)(const TimeMessageBinary *time_msg);

// Called when the connection state changes.
typedef void (*conn_state_handler_t)(connection_state_t new_state);

// =============================================================================
// SerialHandler
// =============================================================================

class SerialHandler {
public:
    SerialHandler();

    // Initialize with the HardwareSerial port (Serial by default on UART0).
    // baud_rate defaults to 115200.
    void begin(unsigned long baud_rate = 115200);

    // Call in loop(): reads available bytes, decodes frames, dispatches messages,
    // sends periodic heartbeat, checks for connection timeout.
    void process();

    // Send a raw frame.
    void send_frame(uint8_t msg_type, const uint8_t *payload, unsigned int len);

    // Send an ACK for a given message type.
    void send_ack(uint8_t acked_type);

    // Send a NACK with an error code.
    void send_nack(uint8_t nacked_type, uint8_t error_code);

    // Send a heartbeat (ping).
    void send_heartbeat();

    // Register handlers
    void on_time(time_handler_t cb)       { time_handler_ = cb; }
    void on_conn_state(conn_state_handler_t cb) { conn_handler_ = cb; }

    // Current connection state
    connection_state_t state() const { return state_; }

    // Protocol version
    static constexpr uint8_t PROTO_MAJOR = 1;
    static constexpr uint8_t PROTO_MINOR = 0;

private:
    frame_decoder_t decoder_;
    connection_state_t state_;
    uint8_t hb_sequence_;
    uint32_t last_hb_sent_;      // millis() of last heartbeat sent
    uint32_t last_hb_received_;  // millis() of last heartbeat received
    time_handler_t time_handler_;
    conn_state_handler_t conn_handler_;

    static constexpr uint32_t HB_INTERVAL_MS   = 5000;  // send heartbeat every 5s
    static constexpr uint32_t HB_TIMEOUT_MS    = 15000; // consider lost after 15s no response

    // Internal frame callback (static trampoline)
    static void on_frame_static(uint8_t type, const uint8_t *payload, unsigned int len);
    void handle_frame(uint8_t type, const uint8_t *payload, unsigned int len);

    // Internal error callback
    static void on_error_static(uint8_t error_code, uint8_t msg_type);
    void handle_error(uint8_t error_code, uint8_t msg_type);

    void set_state(connection_state_t s);
    void check_connection();
};

// Global instance accessor (defined in serial_handler.cpp)
extern SerialHandler g_serial;

#endif // SERIAL_HANDLER_H
