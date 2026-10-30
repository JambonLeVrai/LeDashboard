#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>

// =============================================================================
// Protocol Constants
// =============================================================================

// Message types
enum class MsgType : uint8_t {
    HEARTBEAT    = 0x00,
    TIME         = 0x01,
    WEATHER      = 0x02,
    MEDIA        = 0x03,
    NOTIFICATION = 0x04,
    PORTFOLIO    = 0x05,
    SYSTEM       = 0x06,
    CALENDAR     = 0x07,
    ACK          = 0x08,
    NACK         = 0x09,
    MSG_ERROR    = 0x0A,
    MSG_CONFIG   = 0x0B,
    MSG_REQUEST  = 0x0C,
    MSG_TOUCH    = 0x0D,
    MSG_COMMAND  = 0x0E,
    MSG_DEBUG    = 0x0F,
};

// Frame markers
constexpr uint8_t FRAME_SOF = 0xAA;
constexpr uint8_t FRAME_EOF = 0x55;
constexpr uint8_t FRAME_ESC = 0x5A;

constexpr size_t MAX_PAYLOAD_SIZE = 4095;

// =============================================================================
// CRC-16/CCITT
// =============================================================================

uint16_t crc16_ccitt(const uint8_t *buf, size_t len, uint16_t crc = 0x0000);

// =============================================================================
// Frame Encoding
// =============================================================================

// Encode a message into a complete frame with byte stuffing.
std::vector<uint8_t> encode_frame(MsgType type, const uint8_t *payload, size_t payload_len);

inline std::vector<uint8_t> encode_frame(MsgType type, const std::vector<uint8_t> &payload) {
    return encode_frame(type, payload.data(), payload.size());
}

// =============================================================================
// Frame Decoding (incremental)
// =============================================================================

struct DecodedFrame {
    MsgType type;
    std::vector<uint8_t> payload;
};

// Callback types
// on_frame: called for each valid frame
// on_error: called on CRC mismatch (error_code, raw_type)
using FrameCallback = std::function<void(const DecodedFrame &)>;
using ErrorCallback = std::function<void(uint8_t error_code, uint8_t msg_type)>;

class FrameDecoder {
public:
    FrameDecoder(FrameCallback on_frame, ErrorCallback on_error = nullptr);

    // Feed raw bytes; returns number of complete frames decoded.
    size_t feed(const uint8_t *data, size_t len);

    void reset();

private:
    enum class State { WAIT_SOF, READ_BODY, EXPECT_EOF };

    State state_ = State::WAIT_SOF;
    std::vector<uint8_t> buf_;       // unstuffed body: type + length(2) + payload + crc(2)
    size_t payload_len_ = 0;
    bool esc_ = false;
    FrameCallback on_frame_;
    ErrorCallback on_error_;

    void finish_frame();
};

// =============================================================================
// Message Payload Structures (Binary Format)
// =============================================================================

#pragma pack(push, 1)
struct TimeMessageBinary {
    uint64_t timestamp_ms;    // 8 bytes — Unix timestamp in milliseconds (UTC)
    int16_t  timezone_min;    // 2 bytes — local timezone offset from UTC in minutes
    uint8_t  dst_flag;        // 1 byte  — DST active (0/1)
    // Pre-computed local time fields for direct display:
    uint8_t  hour;            // 1 byte
    uint8_t  minute;          // 1 byte
    uint8_t  second;          // 1 byte
    uint8_t  day;             // 1 byte
    uint8_t  month;           // 1 byte
    uint16_t year;            // 2 bytes (little-endian in packed struct)
    uint8_t  day_of_week;     // 1 byte  — 0=Monday .. 6=Sunday
};  // total: 18 bytes

struct HeartbeatMessage {
    uint8_t sequence;
    uint8_t protocol_major;
    uint8_t protocol_minor;
};

struct AckMessage {
    uint8_t acked_type;
    uint8_t sequence;
};

struct NackMessage {
    uint8_t nacked_type;
    uint8_t error_code;
    uint8_t sequence;
};
#pragma pack(pop)

// Error codes
constexpr uint8_t ERR_CRC_MISMATCH = 0x02;

// Protocol version
constexpr uint8_t PROTOCOL_MAJOR = 1;
constexpr uint8_t PROTOCOL_MINOR = 0;
