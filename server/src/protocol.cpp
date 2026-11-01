#include "protocol.h"
#include <cstring>

// Discovery magic strings
const char DISCOVER_MAGIC[]       = "LEDASHBOARD_PROBE";
const char DISCOVER_ACK_PAYLOAD[] = "LEDASHBOARD_ESP32_V1";

// =============================================================================
// CRC-16/CCITT (poly 0x1021, init 0x0000, no final XOR, no bit reversal)
// =============================================================================

uint16_t crc16_ccitt(const uint8_t *buf, size_t len, uint16_t crc) {
    for (size_t i = 0; i < len; i++) {
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

static void stuff_into(std::vector<uint8_t> &out, const uint8_t *src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = src[i];
        if (b == FRAME_SOF || b == FRAME_EOF || b == FRAME_ESC) {
            out.push_back(FRAME_ESC);
            out.push_back(b ^ 0xA5);
        } else {
            out.push_back(b);
        }
    }
}

std::vector<uint8_t> encode_frame(MsgType type, const uint8_t *payload, size_t payload_len) {
    std::vector<uint8_t> frame;
    frame.reserve(1 + (3 + payload_len + 2) * 2 + 1);

    frame.push_back(FRAME_SOF);

    // Build unstuffed body: type + length(BE) + payload
    uint8_t header[3] = {
        static_cast<uint8_t>(type),
        static_cast<uint8_t>((payload_len >> 8) & 0xFF),
        static_cast<uint8_t>(payload_len & 0xFF)
    };

    // CRC over type + length + payload
    uint16_t crc = crc16_ccitt(header, 3);
    if (payload_len > 0 && payload != nullptr) {
        crc = crc16_ccitt(payload, payload_len, crc);
    }

    // Stuff the body (type + length + payload + crc)
    std::vector<uint8_t> body;
    body.reserve(3 + payload_len + 2);
    body.insert(body.end(), header, header + 3);
    if (payload_len > 0 && payload != nullptr) {
        body.insert(body.end(), payload, payload + payload_len);
    }
    body.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    body.push_back(static_cast<uint8_t>(crc & 0xFF));

    stuff_into(frame, body.data(), body.size());
    frame.push_back(FRAME_EOF);

    return frame;
}

// =============================================================================
// Frame Decoding
// =============================================================================

FrameDecoder::FrameDecoder(FrameCallback on_frame, ErrorCallback on_error)
    : on_frame_(std::move(on_frame)), on_error_(std::move(on_error)) {
    buf_.reserve(MAX_PAYLOAD_SIZE + 5);
}

void FrameDecoder::reset() {
    state_ = State::WAIT_SOF;
    buf_.clear();
    payload_len_ = 0;
    esc_ = false;
}

void FrameDecoder::finish_frame() {
    if (buf_.size() < 5) { reset(); return; } // need at least type+len(2)+crc(2)

    uint8_t msg_type = buf_[0];
    size_t plen = (static_cast<size_t>(buf_[1]) << 8) | buf_[2];
    if (plen > MAX_PAYLOAD_SIZE || buf_.size() < 3 + plen + 2) { reset(); return; }

    uint16_t recv_crc = (static_cast<uint16_t>(buf_[3 + plen]) << 8) | buf_[3 + plen + 1];
    uint16_t calc_crc = crc16_ccitt(buf_.data(), 3 + plen);

    if (calc_crc == recv_crc) {
        DecodedFrame frame;
        frame.type = static_cast<MsgType>(msg_type);
        frame.payload.assign(buf_.begin() + 3, buf_.begin() + 3 + plen);
        if (on_frame_) on_frame_(frame);
    } else {
        if (on_error_) on_error_(ERR_CRC_MISMATCH, msg_type);
    }
    reset();
}

size_t FrameDecoder::feed(const uint8_t *data, size_t len) {
    size_t frames = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t raw = data[i];

        if (state_ == State::WAIT_SOF) {
            if (raw == FRAME_SOF) {
                reset();
                state_ = State::READ_BODY;
            }
            continue;
        }

        // Unstuff
        uint8_t b;
        if (esc_) {
            b = raw ^ 0xA5;
            esc_ = false;
        } else if (raw == FRAME_ESC) {
            esc_ = true;
            continue;
        } else if (raw == FRAME_EOF && state_ == State::EXPECT_EOF) {
            finish_frame();
            frames++;
            continue;
        } else {
            b = raw;
        }

        if (state_ == State::READ_BODY) {
            if (buf_.size() >= MAX_PAYLOAD_SIZE + 5) { reset(); continue; }
            buf_.push_back(b);
            if (buf_.size() == 3) {
                payload_len_ = (static_cast<size_t>(buf_[1]) << 8) | buf_[2];
                if (payload_len_ > MAX_PAYLOAD_SIZE) { reset(); continue; }
            }
            if (buf_.size() == 3 + payload_len_ + 2) {
                state_ = State::EXPECT_EOF;
            }
        } else {
            // Stray byte between body and EOF
            reset();
            if (b == FRAME_SOF) state_ = State::READ_BODY;
        }
    }
    return frames;
}
