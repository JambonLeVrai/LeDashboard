#include "media_source.h"
#include <spdlog/spdlog.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <chrono>
#include <cstring>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

// Pimpl holding the WinRT session manager + event subscriptions
struct MediaSource::Impl {
    GlobalSystemMediaTransportControlsSessionManager mgr{nullptr};
    GlobalSystemMediaTransportControlsSession session{nullptr};
    MediaSource *owner = nullptr;
};

MediaSource::MediaSource() {
    try {
        winrt::init_apartment();
        impl_ = new Impl();
        impl_->owner = this;
        impl_->mgr = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

        // Event: any session appears/disappears/changes
        impl_->mgr.SessionsChanged([this](auto &&, auto &&) {
            changed_ = true;
            spdlog::debug("Media: SMTC SessionsChanged event");
        });
    } catch (const winrt::hresult_error &e) {
        spdlog::warn("Media: failed to init SMTC: {}", winrt::to_string(e.message()));
        delete impl_;
        impl_ = nullptr;
    }
}

MediaSource::~MediaSource() {
    delete impl_;
}

bool MediaSource::update() {
    if (!impl_ || !impl_->mgr) return false;

    try {
        auto session = impl_->mgr.GetCurrentSession();
        prev_ = last_;

        if (!session) {
            last_ = MediaInfo{};
            changed_ = prev_.valid || prev_.state != MEDIA_STOPPED || !prev_.title.empty();
            return true;
        }

        // Subscribe to per-session events when the session changes
        if (session != impl_->session) {
            impl_->session = session;
            session.PlaybackInfoChanged([this](auto &&, auto &&) {
                changed_ = true;
                spdlog::debug("Media: PlaybackInfoChanged event");
            });
            session.MediaPropertiesChanged([this](auto &&, auto &&) {
                changed_ = true;
                spdlog::debug("Media: MediaPropertiesChanged event");
            });
            session.TimelinePropertiesChanged([this](auto &&, auto &&) {
                // Timeline updates don't need an immediate send — 1s tick covers it
            });
        }

        // Source app ID
        std::string source = winrt::to_string(session.SourceAppUserModelId());
        last_.is_spotify = (source.find("Spotify") != std::string::npos);

        // Media properties
        auto props = session.TryGetMediaPropertiesAsync().get();
        last_.title  = winrt::to_string(props.Title());
        last_.artist = winrt::to_string(props.Artist());

        // Playback status
        auto playback = session.GetPlaybackInfo();
        switch (playback.PlaybackStatus()) {
            case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:
                last_.state = MEDIA_PLAYING; break;
            case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:
                last_.state = MEDIA_PAUSED; break;
            default:
                last_.state = MEDIA_STOPPED; break;
        }

        // Timeline — raw position from SMTC
        auto timeline = session.GetTimelineProperties();
        last_.position_ms = static_cast<uint32_t>(timeline.Position().count() / 10000);
        last_.duration_ms = static_cast<uint32_t>(timeline.EndTime().count() / 10000);

        last_.valid = true;

        changed_ = changed_ || (last_.state != prev_.state) ||
                   (last_.title != prev_.title) ||
                   (last_.artist != prev_.artist) ||
                   (last_.valid != prev_.valid);

        return true;

    } catch (const winrt::hresult_error &e) {
        spdlog::debug("Media: SMTC error: {}", winrt::to_string(e.message()));
        return false;
    } catch (...) {
        return false;
    }
}

std::vector<uint8_t> MediaSource::build_payload() const {
    MediaMessageBinary hdr{};
    hdr.timestamp_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    // Raw SMTC position — ESP32 adds elapsed from timestamp_ms.
    // Single source of truth for elapsed time = the receiver.
    hdr.position_ms = last_.position_ms;
    hdr.duration_ms = last_.duration_ms;
    hdr.state = last_.state;

    std::string title  = last_.title.substr(0, 255);
    std::string artist = last_.artist.substr(0, 255);
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
