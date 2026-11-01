#include "server.h"
#include <spdlog/spdlog.h>

#include <ctime>
#include <cstring>

DashboardServer::DashboardServer()
    : decoder_(
        [this](const DecodedFrame &f) { on_frame(f); },
        [this](uint8_t ec, uint8_t mt) { on_decode_error(ec, mt); }
    ) {}

DashboardServer::~DashboardServer() {
    stop();
    weather_.stop();
    media_.stop();
    system_.stop();
    serial_.close();
}

bool DashboardServer::initialize(const std::string &port_name, uint32_t baud_rate,
                                 const std::string &weather_location) {
    if (!serial_.open(port_name, baud_rate)) {
        spdlog::error("Failed to open serial port {}", port_name);
        return false;
    }

    serial_.set_data_callback([this](const uint8_t *data, size_t len) {
        on_serial_data(data, len);
    });

    // Initialize weather source — starts its own background fetch thread
    if (!weather_.initialize(weather_location)) {
        spdlog::warn("Weather source init failed: {} — continuing without weather", weather_.last_error());
    } else {
        weather_.start(); // background thread, fetches immediately then hourly
    }

    auto now = std::chrono::steady_clock::now();
    last_hb_sent_       = now - HB_INTERVAL;
    last_hb_received_   = now;
    last_time_sent_     = now - TIME_INTERVAL;
    last_media_sent_    = now - MEDIA_INTERVAL;
    last_system_sent_   = now - SYSTEM_INTERVAL;

    // Start media polling thread (runs independently, doesn't block main loop)
    media_.start();

    // Start system monitor thread (samples every 5s)
    system_.start();

    spdlog::info("DashboardServer initialized on {} @ {} (weather: {})", port_name, baud_rate, weather_location);
    return true;
}

void DashboardServer::run() {
    running_ = true;
    spdlog::info("DashboardServer main loop started");

    while (running_) {
        auto now = std::chrono::steady_clock::now();

        // Send any queued responses from the read thread (ACKs, TIME sync, etc.)
        flush_responses();

        // Send heartbeat
        if (now - last_hb_sent_ >= HB_INTERVAL) {
            send_heartbeat();
            last_hb_sent_ = now;
        }

        // Send time
        if (now - last_time_sent_ >= TIME_INTERVAL) {
            send_time();
            last_time_sent_ = now;
        }

        // Media: send immediately on state/track change, otherwise every 1s for timer
        if (media_.changed()) {
            send_media();
            last_media_sent_ = now;
            media_.clear_changed();
        }
        if (now - last_media_sent_ >= MEDIA_INTERVAL) {
            send_media();
            last_media_sent_ = now;
        }

        // System: send every 5s
        if (now - last_system_sent_ >= SYSTEM_INTERVAL) {
            send_system();
            last_system_sent_ = now;
        }

        // Weather: send when new data arrives from the background thread
        if (weather_.dirty() && weather_.has_data()) {
            weather_dirty_ = true;
            weather_.clear_dirty();
        }
        if (weather_dirty_ && weather_.has_data()) {
            send_weather();
            weather_dirty_ = false;
        }

        // Check connection timeout
        auto hb_age = now - last_hb_received_;
        if (connected_ && hb_age > HB_TIMEOUT) {
            connected_ = false;
            spdlog::warn("Connection to ESP32 lost (no heartbeat for {} ms)",
                         std::chrono::duration_cast<std::chrono::milliseconds>(hb_age).count());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    spdlog::info("DashboardServer main loop stopped");
}

void DashboardServer::stop() {
    running_ = false;
}

// =============================================================================
// Sending
// =============================================================================

void DashboardServer::send_frame(MsgType type, const uint8_t *payload, size_t len) {
    auto frame = encode_frame(type, payload, len);
    serial_.write(frame); // queued, non-blocking
}

void DashboardServer::send_heartbeat() {
    HeartbeatMessage hb{};
    hb.sequence       = hb_sequence_++;
    hb.protocol_major = PROTOCOL_MAJOR;
    hb.protocol_minor = PROTOCOL_MINOR;
    send_frame(MsgType::HEARTBEAT, reinterpret_cast<const uint8_t *>(&hb), sizeof(hb));
    spdlog::debug("-> HEARTBEAT seq={}", hb.sequence);
}

TimeMessageBinary DashboardServer::build_time_message() const {
    TimeMessageBinary t{};

    // High-precision UTC timestamp in milliseconds
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    t.timestamp_ms = static_cast<uint64_t>(now_ms.time_since_epoch().count());

    // Local time fields (for direct display)
    std::time_t now_t = std::time(nullptr);
    std::tm tm_local{};
    localtime_s(&tm_local, &now_t);

    t.hour        = static_cast<uint8_t>(tm_local.tm_hour);
    t.minute      = static_cast<uint8_t>(tm_local.tm_min);
    t.second      = static_cast<uint8_t>(tm_local.tm_sec);
    t.day         = static_cast<uint8_t>(tm_local.tm_mday);
    t.month       = static_cast<uint8_t>(tm_local.tm_mon + 1);
    t.year        = static_cast<uint16_t>(tm_local.tm_year + 1900);
    t.day_of_week = static_cast<uint8_t>((tm_local.tm_wday + 6) % 7); // 0=Mon..6=Sun

    // Timezone offset from UTC in minutes (including DST if active)
#ifdef _WIN32
    TIME_ZONE_INFORMATION tzi{};
    DWORD tz_result = GetTimeZoneInformation(&tzi);
    int16_t bias = static_cast<int16_t>(tzi.Bias); // minutes behind UTC
    if (tz_result == TIME_ZONE_ID_DAYLIGHT) {
        bias += static_cast<int16_t>(tzi.DaylightBias);
    }
    t.timezone_min = static_cast<int16_t>(-bias); // convert to "minutes east of UTC"
    t.dst_flag = (tz_result == TIME_ZONE_ID_DAYLIGHT) ? 1 : 0;
#else
    t.timezone_min = 0;
    t.dst_flag = 0;
#endif

    return t;
}

void DashboardServer::send_time() {
    TimeMessageBinary t = build_time_message();
    send_frame(MsgType::TIME, reinterpret_cast<const uint8_t *>(&t), sizeof(t));
    spdlog::debug("-> TIME {:02d}:{:02d}:{:02d} {:02d}/{:02d}/{:04d} dow={} ts_ms={}",
                  t.hour, t.minute, t.second, t.day, t.month, t.year, t.day_of_week,
                  t.timestamp_ms);
}

void DashboardServer::send_weather() {
    auto w = weather_.last_message();
    send_frame(MsgType::WEATHER, reinterpret_cast<const uint8_t *>(&w), sizeof(w));
    spdlog::debug("-> WEATHER temp={:.1f}C feels={:.1f}C RH={}% cond={} UV={} AQI={} "
                  "tmr={:.1f}/{:.1f}C cond={}",
                  w.temp_x10 / 10.0, w.feels_like_x10 / 10.0, w.humidity,
                  w.condition_id, w.uv_index, w.air_quality,
                  w.tomorrow_min_x10 / 10.0, w.tomorrow_max_x10 / 10.0,
                  w.tomorrow_condition);
}

void DashboardServer::send_media() {
    auto payload = media_.build_payload();
    send_frame(MsgType::MEDIA, payload);
    auto m = media_.snapshot();
    if (m.valid) {
        auto fmt = [](uint32_t ms) {
            return fmt::format("{}:{:02d}", ms / 60000, (ms / 1000) % 60);
        };
        spdlog::debug("-> MEDIA [{}] {}/{} \"{}\" — \"{}\"{}",
                      m.state == MEDIA_PLAYING ? "PLAY" : m.state == MEDIA_PAUSED ? "PAUSE" : "STOP",
                      fmt(m.position_ms), fmt(m.duration_ms),
                      m.title, m.artist, m.is_spotify ? " [spotify]" : "");
    } else {
        spdlog::debug("-> MEDIA [STOP] no active session");
    }
}

void DashboardServer::send_system() {
    auto s = system_.snapshot();
    send_frame(MsgType::SYSTEM, reinterpret_cast<const uint8_t *>(&s), sizeof(s));
    spdlog::debug("-> SYSTEM CPU={:.0f}%/{:.0f}C GPU={:.0f}%/{:.0f}C RAM={}/{}MB DiskIO={:.0f}% NET={:.1f}/{:.1f}Mbps",
                  s.cpu_x10 / 10.0, s.cpu_temp_x10 / 10.0,
                  s.gpu_x10 / 10.0, s.gpu_temp_x10 / 10.0,
                  s.ram_used_mb, s.ram_total_mb, s.disk_io_x10 / 10.0,
                  s.net_down_mbps_x10 / 10.0, s.net_up_mbps_x10 / 10.0);
}

void DashboardServer::send_ack(MsgType acked_type) {
    AckMessage ack{};
    ack.acked_type = static_cast<uint8_t>(acked_type);
    ack.sequence   = 0;
    send_frame(MsgType::ACK, reinterpret_cast<const uint8_t *>(&ack), sizeof(ack));
    spdlog::debug("-> ACK type=0x{:02X}", static_cast<uint8_t>(acked_type));
}

void DashboardServer::send_nack(MsgType nacked_type, uint8_t error_code) {
    NackMessage nack{};
    nack.nacked_type = static_cast<uint8_t>(nacked_type);
    nack.error_code  = error_code;
    nack.sequence    = 0;
    send_frame(MsgType::NACK, reinterpret_cast<const uint8_t *>(&nack), sizeof(nack));
    spdlog::debug("-> NACK type=0x{:02X} err=0x{:02X}",
                  static_cast<uint8_t>(nacked_type), error_code);
}

// =============================================================================
// Receiving
// =============================================================================

void DashboardServer::on_serial_data(const uint8_t *data, size_t len) {
    decoder_.feed(data, len);
}

void DashboardServer::queue_response(MsgType type, const uint8_t *payload, size_t len) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    response_queue_.emplace(type, std::vector<uint8_t>(payload, payload + len));
}

void DashboardServer::flush_responses() {
    std::queue<std::pair<MsgType, std::vector<uint8_t>>> q;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        q.swap(response_queue_);
    }
    while (!q.empty()) {
        send_frame(q.front().first, q.front().second);
        q.pop();
    }
}

void DashboardServer::on_frame(const DecodedFrame &frame) {
    const char *type_name = "UNKNOWN";
    switch (frame.type) {
        case MsgType::HEARTBEAT:    type_name = "HEARTBEAT"; break;
        case MsgType::TIME:         type_name = "TIME"; break;
        case MsgType::WEATHER:      type_name = "WEATHER"; break;
        case MsgType::MEDIA:        type_name = "MEDIA"; break;
        case MsgType::NOTIFICATION: type_name = "NOTIFICATION"; break;
        case MsgType::PORTFOLIO:    type_name = "PORTFOLIO"; break;
        case MsgType::SYSTEM:       type_name = "SYSTEM"; break;
        case MsgType::CALENDAR:     type_name = "CALENDAR"; break;
        case MsgType::ACK:          type_name = "ACK"; break;
        case MsgType::NACK:         type_name = "NACK"; break;
        case MsgType::MSG_ERROR:    type_name = "ERROR"; break;
        case MsgType::MSG_CONFIG:   type_name = "CONFIG"; break;
        case MsgType::MSG_REQUEST:  type_name = "REQUEST"; break;
        case MsgType::MSG_TOUCH:    type_name = "TOUCH"; break;
        case MsgType::MSG_COMMAND:  type_name = "COMMAND"; break;
        case MsgType::MSG_DEBUG:    type_name = "DEBUG"; break;
    }
    spdlog::debug("<- {} ({} bytes)", type_name, frame.payload.size());

    switch (frame.type) {
        case MsgType::HEARTBEAT: {
            last_hb_received_ = std::chrono::steady_clock::now();
            if (!connected_) {
                connected_ = true;
                weather_dirty_ = true;
                // Queue TIME + MEDIA for immediate send by main loop
                TimeMessageBinary t = build_time_message();
                queue_response(MsgType::TIME, reinterpret_cast<const uint8_t *>(&t), sizeof(t));
                queue_response(MsgType::MEDIA, media_.build_payload());
                spdlog::info("ESP32 connected (heartbeat received, time+weather+media queued)");
            }
            uint8_t seq = frame.payload.size() > 0 ? frame.payload[0] : 0;
            spdlog::debug("   HEARTBEAT seq={}", seq);
            break;
        }

        case MsgType::ACK: {
            if (frame.payload.size() >= 1) {
                spdlog::debug("   ACK for type=0x{:02X}", frame.payload[0]);
            }
            break;
        }

        case MsgType::NACK: {
            if (frame.payload.size() >= 2) {
                spdlog::warn("   NACK for type=0x{:02X} err=0x{:02X}",
                             frame.payload[0], frame.payload[1]);
            }
            break;
        }

        case MsgType::MSG_REQUEST: {
            if (!frame.payload.empty()) {
                uint8_t req = frame.payload[0];
                spdlog::info("   REQUEST type={}", req);
                if (req == 0x01) {
                    // Queue TIME sync response
                    TimeMessageBinary t = build_time_message();
                    queue_response(MsgType::TIME, reinterpret_cast<const uint8_t *>(&t), sizeof(t));
                }
            }
            AckMessage ack{};
            ack.acked_type = static_cast<uint8_t>(MsgType::MSG_REQUEST);
            ack.sequence = 0;
            queue_response(MsgType::ACK, reinterpret_cast<const uint8_t *>(&ack), sizeof(ack));
            break;
        }

        case MsgType::MSG_TOUCH: {
            if (frame.payload.size() >= 9) {
                uint16_t x = (frame.payload[1] << 8) | frame.payload[2];
                uint16_t y = (frame.payload[3] << 8) | frame.payload[4];
                spdlog::info("   TOUCH event={} x={} y={}", frame.payload[0], x, y);
            }
            AckMessage ack{};
            ack.acked_type = static_cast<uint8_t>(MsgType::MSG_TOUCH);
            queue_response(MsgType::ACK, reinterpret_cast<const uint8_t *>(&ack), sizeof(ack));
            break;
        }

        default: {
            AckMessage ack{};
            ack.acked_type = static_cast<uint8_t>(frame.type);
            queue_response(MsgType::ACK, reinterpret_cast<const uint8_t *>(&ack), sizeof(ack));
            break;
        }
    }
}

void DashboardServer::on_decode_error(uint8_t error_code, uint8_t msg_type) {
    spdlog::warn("CRC error on type=0x{:02X}", msg_type);
    NackMessage nack{};
    nack.nacked_type = msg_type;
    nack.error_code  = error_code;
    nack.sequence    = 0;
    queue_response(MsgType::NACK, reinterpret_cast<const uint8_t *>(&nack), sizeof(nack));
}
