#include "ts3toolkit/plugin/audio_playback_adapter.h"

namespace ts3toolkit::plugin {

AudioPlaybackAdapter::AudioPlaybackAdapter(audio::AudioEngine& engine)
    : engine_(engine) {}

void AudioPlaybackAdapter::startOverlay(const audio::DecodedSound& sound,
                                        float volume) {
    engine_.playSound(sound, volume);
}

void AudioPlaybackAdapter::stopOverlay() {
    engine_.stopSound();
}

bool AudioPlaybackAdapter::isOverlayPlaying() const {
    return engine_.state() == audio::PlaybackState::kPlaying;
}

}  // namespace ts3toolkit::plugin
