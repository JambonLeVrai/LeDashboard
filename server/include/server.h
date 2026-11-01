#pragma once

#include "serial_port.h"
#include "protocol.h"
#include "weather_source.h"

#include <string>
#include <atomic>
#include <chrono>
#include <mutex>

// =============================================================================
// DashboardServer — main server logic
// =============================================================================
//
// Opens a serial port, sends HEARTBEAT every 5s and TIME every 1s,
// fetches weather from Open-Meteo every 10min and sends WEATHER frames,
// decodes incoming frames (ACK, NACK, HEARTBEAT, TOUCH, REQUEST, ...).

class DashboardServer {
public:
    DashboardServer();
    ~DashboardServer();

    // Initialize: open serial port with given settings.
    // weather_location: city name or "lat,lon" for Open-Meteo.
    bool initialize(const std::string &port_name, uint32_t baud_rate = 115200,
                    const std::string &weather_location = "Paris");

    // Main loop: runs until stop() is called (blocks).
    void run();

    // Signal the main loop to stop.
    void stop();

    bool is_running() const { return running_; }
    bool is_connected() const { return connected_; }

private:
    // Send helpers
    void send_frame(MsgType type, const uint8_t *payload, size_t len);
    void send_frame(MsgType type, const std::vector<uint8_t> &payload) {
        send_frame(type, payload.data(), payload.size());
    }
    void send_heartbeat();
    void send_time();
    void send_weather();
    void send_ack(MsgType acked_type);
    void send_nack(MsgType nacked_type, uint8_t error_code);

    // Incoming data handler (called from SerialPort read thread)
    void on_serial_data(const uint8_t *data, size_t len);
    void on_frame(const DecodedFrame &frame);
    void on_decode_error(uint8_t error_code, uint8_t msg_type);

    // Build a TIME message from the current system time
    TimeMessageBinary build_time_message() const;

    SerialPort serial_;
    FrameDecoder decoder_;
    WeatherSource weather_;

    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    uint8_t hb_sequence_ = 0;
    std::chrono::steady_clock::time_point last_hb_sent_;
    std::chrono::steady_clock::time_point last_hb_received_;
    std::chrono::steady_clock::time_point last_time_sent_;
    std::chrono::steady_clock::time_point last_weather_sent_;

    static constexpr auto HB_INTERVAL      = std::chrono::seconds(5);
    static constexpr auto TIME_INTERVAL    = std::chrono::seconds(1);
    static constexpr auto WEATHER_INTERVAL = std::chrono::seconds(1); // send if data available; refetch every 10min internally
    static constexpr auto HB_TIMEOUT       = std::chrono::seconds(15);

    std::mutex send_mutex_;
};
