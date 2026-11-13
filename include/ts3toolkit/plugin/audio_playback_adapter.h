#pragma once

#include "ts3toolkit/audio/audio_engine.h"
#include "ts3toolkit/entrance/entrance_controller.h"

namespace ts3toolkit::plugin {

// Adapts AudioEngine to the EntranceController::Playback port. This is the
// production wiring that preserves the dependency rule: the entrance layer
// declares the abstract port and never references the audio engine directly.
class AudioPlaybackAdapter : public entrance::EntranceController::Playback {
public:
    explicit AudioPlaybackAdapter(audio::AudioEngine& engine);

    void startOverlay(const audio::DecodedSound& sound, float volume) override;
    void stopOverlay() override;
    bool isOverlayPlaying() const override;

private:
    audio::AudioEngine& engine_;
};

}  // namespace ts3toolkit::plugin
