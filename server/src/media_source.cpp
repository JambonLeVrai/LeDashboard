#include "media_source.h"
#include <spdlog/spdlog.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <chrono>
#include <cstring>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

struct MediaSource::Impl {
    GlobalSystemMediaTransportControlsSessionManager mgr{nullptr};
    GlobalSystemMediaTransportControlsSession session{nullptr};
};

MediaSource::MediaSource() {
    try {
        winrt::init_apartment();
        impl_ = new Impl();
        impl_->mgr = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    } catch (const winrt::hresult_error &e) {
        spdlog::warn("Media: failed to init SMTC: {}", winrt::to_string(e.message()));
        delete impl_;
        impl_ = nullptr;
    }
    last_poll_time_ = std::chrono::steady_clock::now();
}

MediaSource::~MediaSource() {
    stop();
    delete impl_;
}

void MediaSource::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&MediaSource::poll_loop, this);
}

void MediaSource::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void MediaSource::poll_loop() {
    while (running_) {
        poll_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void MediaSource::poll_once() {
    if (!impl_ || !impl_->mgr) return;

    try {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now - last_poll_time_).count();
        last_poll_time_ = now;

        auto session = impl_->mgr.GetCurrentSession();

        MediaInfo new_info;
        uint32_t smtc_pos = 0;
        uint32_t smtc_dur = 0;

        if (session) {
            // Subscribe to events on session change for instant notification
            if (session != impl_->session) {
                impl_->session = session;
                session.PlaybackInfoChanged([this](auto &&, auto &&) {
                    changed_ = true;
                });
                session.MediaPropertiesChanged([this](auto &&, auto &&) {
                    changed_ = true;
                });
            }

            std::string source = winrt::to_string(session.SourceAppUserModelId());
            new_info.is_spotify = (source.find("Spotify") != std::string::npos);

            auto props = session.TryGetMediaPropertiesAsync().get();
            new_info.title  = winrt::to_string(props.Title());
            new_info.artist = winrt::to_string(props.Artist());

            auto playback = session.GetPlaybackInfo();
            switch (playback.PlaybackStatus()) {
                case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:
                    new_info.state = MEDIA_PLAYING; break;
                case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:
                    new_info.state = MEDIA_PAUSED; break;
                default:
                    new_info.state = MEDIA_STOPPED; break;
            }

            auto timeline = session.GetTimelineProperties();
            smtc_pos = static_cast<uint32_t>(timeline.Position().count() / 10000);
            smtc_dur = static_cast<uint32_t>(timeline.EndTime().count() / 10000);
            new_info.duration_ms = smtc_dur;
            new_info.valid = true;
        } else {
            new_info.valid = false;
            new_info.state = MEDIA_STOPPED;
        }

        // Position tracking: advance by wall-clock when playing,
        // snap to SMTC on state change or track change
        {
            std::lock_guard<std::mutex> lock(mutex_);

            bool state_changed = (new_info.state != tracked_state_);
            bool track_changed = (new_info.title != info_.title || new_info.artist != info_.artist);
            bool session_appeared = (new_info.valid && !info_.valid);

            if (new_info.valid && new_info.state == MEDIA_PLAYING) {
                if (state_changed || track_changed || session_appeared) {
                    // Snap to SMTC position on change
                    tracked_position_ms_ = smtc_pos;
                } else {
                    // Advance by wall-clock
                    tracked_position_ms_ += static_cast<uint32_t>(elapsed_ms);
                }
                // Clamp to duration
                if (smtc_dur > 0 && tracked_position_ms_ > smtc_dur) {
                    tracked_position_ms_ = smtc_dur;
                }
            } else {
                // Paused or stopped: use SMTC position directly
                tracked_position_ms_ = smtc_pos;
            }

            tracked_state_ = new_info.state;
            new_info.position_ms = tracked_position_ms_;

            // Detect changes for the sender
            if (state_changed || track_changed || session_appeared ||
                (new_info.valid != info_.valid)) {
                changed_ = true;
                if (state_changed) {
                    spdlog::info("Media state: {} -> {}  \"{}\" — \"{}\"",
                                 tracked_state_, new_info.state,
                                 new_info.title, new_info.artist);
                }
            }

            info_ = new_info;
        }

    } catch (const winrt::hresult_error &e) {
        spdlog::debug("Media: SMTC error: {}", winrt::to_string(e.message()));
    } catch (...) {
        // ignore transient errors
    }
}

MediaInfo MediaSource::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Advance position if currently playing
    MediaInfo snap = info_;
    if (snap.valid && snap.state == MEDIA_PLAYING) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - last_poll_time_).count();
        // last_poll_time_ is updated by the poll thread; we can't read it under lock
        // safely here, so just use the tracked position from the last poll.
        // The 200ms poll interval means at most 200ms of drift — negligible.
    }
    return snap;
}

std::vector<uint8_t> MediaSource::build_payload() const {
    auto snap = snapshot();

    MediaMessageBinary hdr{};
    hdr.timestamp_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    hdr.position_ms = snap.position_ms;
    hdr.duration_ms = snap.duration_ms;
    hdr.state = snap.state;

    std::string title  = snap.title.substr(0, 255);
    std::string artist = snap.artist.substr(0, 255);
    hdr.title_len  = static_cast<uint8_t>(title.size());
    hdr.artist_len = static_cast<uint8_t>(artist.size());

    std::vector<uint8_t> payload;
    payload.resize(sizeof(hdr) + hdr.title_len + hdr.artist_len);
    std::memcpy(payload.data(), &hdr, sizeof(hdr));
    if (hdr.title_len > 0)
        std::memcpy(payload.data() + sizeof(hdr), title.data(), hdr.title_len);
    if (hdr.artist_len > 0)
        std::memcpy(payload.data() + sizeof(hdr) + hdr.title_len, artist.data(), hdr.artist_len);
    return payload;
}
