#include "media_source.h"
#include <spdlog/spdlog.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <chrono>
#include <cstring>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

MediaSource::MediaSource() {
    try {
        winrt::init_apartment();
    } catch (...) {
        // already initialized is fine
    }
}

bool MediaSource::update() {
    try {
        auto mgr = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        auto session = mgr.GetCurrentSession();

        prev_ = last_;

        if (!session) {
            last_ = MediaInfo{};
            last_.valid = false;
            last_.state = MEDIA_STOPPED;
            changed_ = prev_.valid || prev_.state != MEDIA_STOPPED || !prev_.title.empty();
            return true;
        }

        // Source app ID (e.g. "Spotify.exe")
        std::string source = winrt::to_string(session.SourceAppUserModelId());
        last_.is_spotify = (source.find("Spotify") != std::string::npos);

        // Media properties (title, artist)
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

        // Timeline — anchor position with local timestamp for live computation
        auto timeline = session.GetTimelineProperties();
        last_.smtc_position_ms = static_cast<uint32_t>(timeline.Position().count() / 10000);
        last_.duration_ms      = static_cast<uint32_t>(timeline.EndTime().count() / 10000);
        last_.sampled_at_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch()).count();
        last_.position_ms = last_.smtc_position_ms;

        last_.valid = true;

        // Detect changes: state, title, artist, or session appeared/disappeared
        changed_ = (last_.state != prev_.state) ||
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

uint32_t MediaSource::live_position_ms() const {
    if (!last_.valid || last_.state != MEDIA_PLAYING) return last_.position_ms;
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t elapsed = now_ms - last_.sampled_at_ms;
    int64_t pos = (int64_t)last_.smtc_position_ms + elapsed;
    if (last_.duration_ms > 0 && pos > (int64_t)last_.duration_ms) pos = last_.duration_ms;
    if (pos < 0) pos = 0;
    return (uint32_t)pos;
}

std::vector<uint8_t> MediaSource::build_payload() const {
    MediaMessageBinary hdr{};
    hdr.timestamp_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    hdr.position_ms = const_cast<MediaSource*>(this)->live_position_ms();
    hdr.duration_ms = last_.duration_ms;
    hdr.state = last_.state;

    // Clamp strings to fit in uint8_t length fields
    std::string title  = last_.title.substr(0, 255);
    std::string artist = last_.artist.substr(0, 255);
    hdr.title_len  = static_cast<uint8_t>(title.size());
    hdr.artist_len = static_cast<uint8_t>(artist.size());

    std::vector<uint8_t> payload;
    payload.resize(sizeof(hdr) + hdr.title_len + hdr.artist_len);
    std::memcpy(payload.data(), &hdr, sizeof(hdr));
    if (hdr.title_len > 0) {
        std::memcpy(payload.data() + sizeof(hdr), title.data(), hdr.title_len);
    }
    if (hdr.artist_len > 0) {
        std::memcpy(payload.data() + sizeof(hdr) + hdr.title_len, artist.data(), hdr.artist_len);
    }
    return payload;
}
