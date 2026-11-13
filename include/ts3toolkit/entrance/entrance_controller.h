#pragma once

#include "ts3toolkit/audio/decoded_sound.h"
#include "ts3toolkit/config/settings.h"
#include "ts3toolkit/platform/clock.h"

#include <cstdint>

namespace ts3toolkit::entrance {

// Logical state of the entrance sound feature.
enum class ControllerState {
    kIdle,
    kPending,
    kPlaying,
    kCooldown,
};

// Drives the entrance sound state machine. It is purely logical: it decides
// whether a channel entry should play and issues commands to an injected
// playback sink. Decoding happens before scheduling, so no I/O occurs here.
class EntranceController {
public:
    // Sink for playback commands. The production implementation adapts the
    // AudioEngine; tests use a fake.
    class Playback {
    public:
        virtual ~Playback() = default;
        virtual void startOverlay(const audio::DecodedSound& sound,
                                  float volume) = 0;
        virtual void stopOverlay() = 0;
        virtual bool isOverlayPlaying() const = 0;
    };

    EntranceController(const platform::Clock& clock, Playback& playback);

    // Replace cached settings and the pre-decoded sound. A null sound means no
    // playable data is available and triggers fail closed.
    void updateSettings(const config::EntranceSoundSettings& settings,
                        const audio::DecodedSound* prepared_sound);

    // Invoked after the caller confirms the event is the local client moving
    // into a valid, non-zero destination channel.
    void onLocalClientEnteredChannel();
    void cancelPending();

    // Advance playing/cooldown transitions using the injected clock.
    void poll();

    ControllerState state() const;

private:
    const platform::Clock& clock_;
    Playback& playback_;
    config::EntranceSoundSettings settings_;
    const audio::DecodedSound* prepared_sound_;
    ControllerState state_;
    std::int64_t cooldown_start_ms_;
    std::int64_t pending_start_ms_;
};

}  // namespace ts3toolkit::entrance
