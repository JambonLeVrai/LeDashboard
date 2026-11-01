#pragma once

#include "protocol.h"
#include <string>
#include <cstdint>
#include <vector>

// Forward-declare WinRT types to avoid pulling headers into this .h
namespace winrt::Windows::Media::Control {
    struct GlobalSystemMediaTransportControlsSessionManager;
    struct GlobalSystemMediaTransportControlsSession;
}

// =============================================================================
// MediaSource — reads currently-playing media via Windows SMTC
// =============================================================================

struct MediaInfo {
    bool     valid = false;
    bool     is_spotify = false;
    uint8_t  state = MEDIA_STOPPED;
    uint32_t position_ms = 0;       // raw SMTC position
    uint32_t duration_ms = 0;
    std::string title;
    std::string artist;
};

class MediaSource {
public:
    MediaSource();
    ~MediaSource();

    bool update();
    const MediaInfo &last() const { return last_; }
    bool changed() const { return changed_; }
    void clear_changed() { changed_ = false; }

    // Build MEDIA payload: sends raw SMTC position + current timestamp.
    // ESP32 computes elapsed from timestamp_ms (single source of truth).
    std::vector<uint8_t> build_payload() const;

private:
    MediaInfo last_;
    MediaInfo prev_;
    bool changed_ = true;

    // Pimpl to hide WinRT types from this header
    struct Impl;
    Impl *impl_ = nullptr;
};
