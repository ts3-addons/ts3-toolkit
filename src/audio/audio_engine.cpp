#include "ts3toolkit/audio/audio_engine.h"

#include <algorithm>
#include <utility>

#include "ts3toolkit/core/mixer.h"

namespace ts3toolkit::audio {

AudioEngine::AudioEngine(std::size_t capture_ring_capacity)
    : capture_(capture_ring_capacity),
      playing_(false),
      overlay_(),
      overlay_position_(0) {}

AudioEngine::~AudioEngine() {
    stopCapture();
}

bool AudioEngine::startCapture() {
    return capture_.start();
}

void AudioEngine::stopCapture() {
    capture_.stop();
}

bool AudioEngine::captureRunning() const {
    return capture_.isRunning();
}

void AudioEngine::playSound(const DecodedSound& sound, float volume) {
    // Prepare outside the lock; only a short move happens while holding it.
    std::vector<core::Sample> prepared = sound.samples;
    core::applyGain(prepared.data(), prepared.size(), volume);

    std::lock_guard<std::mutex> lock(overlay_mutex_);
    overlay_ = std::move(prepared);
    overlay_position_ = 0;
    playing_.store(true, std::memory_order_release);
}

void AudioEngine::stopSound() {
    std::lock_guard<std::mutex> lock(overlay_mutex_);
    playing_.store(false, std::memory_order_release);
    overlay_position_ = 0;
    overlay_.clear();
}

PlaybackState AudioEngine::state() const {
    return playing_.load(std::memory_order_acquire) ? PlaybackState::kPlaying
                                                    : PlaybackState::kIdle;
}

void AudioEngine::fillFrame(core::Sample* output, std::size_t count) {
    // Microphone passthrough; zero-fill any underrun so the frame is defined.
    const std::size_t obtained = capture_.read(output, count);
    for (std::size_t i = obtained; i < count; ++i) {
        output[i] = 0;
    }

    if (!playing_.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> lock(overlay_mutex_);
    const std::size_t remaining = overlay_.size() - overlay_position_;
    if (remaining == 0) {
        playing_.store(false, std::memory_order_release);
        overlay_position_ = 0;
        return;
    }

    const std::size_t to_mix = (std::min)(count, remaining);
    core::mixAdd(output, overlay_.data() + overlay_position_, to_mix);
    overlay_position_ += to_mix;

    if (overlay_position_ >= overlay_.size()) {
        playing_.store(false, std::memory_order_release);
        overlay_position_ = 0;
    }
}

}  // namespace ts3toolkit::audio
