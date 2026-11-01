#pragma once

#include "protocol.h"
#include <string>
#include <cstdint>

// =============================================================================
// MediaSource — reads currently-playing media via Windows SMTC
// =============================================================================
//
// Uses GlobalSystemMediaTransportControlsSessionManager (Windows 10/11) to
// get the active media session (Spotify, VLC, browser, ...). Filters to
// Spotify when available.

struct MediaInfo {
    bool     valid = false;
    bool     is_spotify = false;
    uint8_t  state = MEDIA_STOPPED;
    uint32_t position_ms = 0;       // live-computed position at last update
    uint32_t duration_ms = 0;
    std::string title;
    std::string artist;

    // Anchor for live position computation: SMTC position + local time when sampled
    uint32_t smtc_position_ms = 0;  // raw position from SMTC
    int64_t  sampled_at_ms = 0;     // system_clock ms when SMTC was queried
};

class MediaSource {
public:
    MediaSource();

    // Poll SMTC for current media. Returns true if info was retrieved.
    bool update();

    const MediaInfo &last() const { return last_; }

    // Did the state or track change since the last update?
    bool changed() const { return changed_; }
    void clear_changed() { changed_ = false; }

    // Compute live position (ms) accounting for elapsed time since last SMTC sample
    uint32_t live_position_ms() const;

    // Build the binary MEDIA message payload (header + title + artist).
    std::vector<uint8_t> build_payload() const;

private:
    MediaInfo last_;
    MediaInfo prev_;
    bool changed_ = true; // force send on first update
};
