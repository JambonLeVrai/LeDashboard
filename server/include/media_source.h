#pragma once

#include "protocol.h"
#include <string>
#include <cstdint>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>

// =============================================================================
// MediaSource — reads currently-playing media via Windows SMTC
// =============================================================================
//
// Runs its own polling thread. Position is tracked server-side:
//   - When PLAYING, position advances by wall-clock time between polls
//   - On state change or track change, position snaps to the SMTC value
// This avoids relying on SMTC's (often stale/buggy for browsers) position.

struct MediaInfo {
    bool     valid = false;
    bool     is_spotify = false;
    uint8_t  state = MEDIA_STOPPED;
    uint32_t position_ms = 0;   // server-tracked position
    uint32_t duration_ms = 0;
    std::string title;
    std::string artist;
};

class MediaSource {
public:
    MediaSource();
    ~MediaSource();

    // Start the background polling thread (200ms interval)
    void start();
    void stop();

    // Snapshot of current media info (thread-safe)
    MediaInfo snapshot() const;

    // True if state/track changed since last clear_changed() call
    bool changed() const { return changed_.load(); }
    void clear_changed() { changed_ = false; }

    // Build MEDIA payload with current server timestamp + tracked position
    std::vector<uint8_t> build_payload() const;

private:
    void poll_loop();
    void poll_once(); // single SMTC query + position update

    mutable std::mutex mutex_;
    MediaInfo info_;
    std::atomic<bool> changed_{true};

    std::atomic<bool> running_{false};
    std::thread thread_;

    // Position tracking
    uint32_t tracked_position_ms_ = 0;
    uint8_t  tracked_state_ = MEDIA_STOPPED;
    uint32_t last_smtc_pos_ = 0;  // last raw SMTC position — detect when app updates it
    std::chrono::steady_clock::time_point last_poll_time_;

    // Pimpl to hide WinRT types
    struct Impl;
    Impl *impl_ = nullptr;
};
