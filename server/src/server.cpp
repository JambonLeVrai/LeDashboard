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
    serial_.close();
}

bool DashboardServer::initialize(const std::string &port_name, uint32_t baud_rate) {
    if (!serial_.open(port_name, baud_rate)) {
        spdlog::error("Failed to open serial port {}", port_name);
        return false;
    }

    serial_.set_data_callback([this](const uint8_t *data, size_t len) {
        on_serial_data(data, len);
    });

    auto now = std::chrono::steady_clock::now();
    last_hb_sent_     = now;
    last_hb_received_ = now;
    last_time_sent_   = now;

    spdlog::info("DashboardServer initialized on {} @ {}", port_name, baud_rate);
    return true;
}

void DashboardServer::run() {
    running_ = true;
    spdlog::info("DashboardServer main loop started");

    while (running_) {
        auto now = std::chrono::steady_clock::now();

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
    std::lock_guard<std::mutex> lock(send_mutex_);
    serial_.write(frame);
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
    std::time_t now = std::time(nullptr);
    std::tm tm_local{};
    localtime_s(&tm_local, &now);

    t.hour        = static_cast<uint8_t>(tm_local.tm_hour);
    t.minute      = static_cast<uint8_t>(tm_local.tm_min);
    t.second      = static_cast<uint8_t>(tm_local.tm_sec);
    t.day         = static_cast<uint8_t>(tm_local.tm_mday);
    t.month       = static_cast<uint8_t>(tm_local.tm_mon + 1);       // tm_mon is 0-based
    t.year        = static_cast<uint16_t>(tm_local.tm_year + 1900);
    // tm_wday: 0=Sunday .. 6=Saturday → convert to 0=Monday .. 6=Sunday
    t.day_of_week = static_cast<uint8_t>((tm_local.tm_wday + 6) % 7);
    return t;
}

void DashboardServer::send_time() {
    TimeMessageBinary t = build_time_message();
    send_frame(MsgType::TIME, reinterpret_cast<const uint8_t *>(&t), sizeof(t));
    spdlog::debug("-> TIME {:02d}:{:02d}:{:02d} {:02d}/{:02d}/{:04d} dow={}",
                  t.hour, t.minute, t.second, t.day, t.month, t.year, t.day_of_week);
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
                spdlog::info("ESP32 connected (heartbeat received)");
            }
            uint8_t seq = frame.payload.size() > 0 ? frame.payload[0] : 0;
            spdlog::debug("   HEARTBEAT seq={}", seq);
            // Do NOT echo — server only sends its own periodic heartbeat every 5s.
            // The ESP32 echoes our heartbeat, which is how we detect it's alive.
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
                // 0x01 = time sync request
                if (req == 0x01) {
                    send_time();
                }
            }
            send_ack(MsgType::MSG_REQUEST);
            break;
        }

        case MsgType::MSG_TOUCH: {
            if (frame.payload.size() >= 9) {
                uint16_t x = (frame.payload[1] << 8) | frame.payload[2];
                uint16_t y = (frame.payload[3] << 8) | frame.payload[4];
                spdlog::info("   TOUCH event={} x={} y={}", frame.payload[0], x, y);
            }
            send_ack(MsgType::MSG_TOUCH);
            break;
        }

        default:
            // ACK any other valid frame
            send_ack(frame.type);
            break;
    }
}

void DashboardServer::on_decode_error(uint8_t error_code, uint8_t msg_type) {
    spdlog::warn("CRC error on type=0x{:02X} -> NACK", msg_type);
    send_nack(static_cast<MsgType>(msg_type), error_code);
}
