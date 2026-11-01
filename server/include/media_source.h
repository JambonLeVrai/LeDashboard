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
    bool     valid = false;       // true if any media session is active
    bool     is_spotify = false;
    uint8_t  state = MEDIA_STOPPED; // 0=stopped, 1=paused, 2=playing
    uint32_t position_ms = 0;
    uint32_t duration_ms = 0;
    std::string title;
    std::string artist;
};

class MediaSource {
public:
    MediaSource();

    // Poll SMTC for current media. Returns true if info was retrieved.
    bool update();

    const MediaInfo &last() const { return last_; }

    // Build the binary MEDIA message payload (header + title + artist).
    // Returns the payload as a byte vector, ready to send in a MEDIA frame.
    std::vector<uint8_t> build_payload() const;

private:
    MediaInfo last_;
};
