#include "ts3toolkit/audio/audio_engine.h"

#include <algorithm>
#include <utility>

#include "ts3toolkit/core/mixer.h"

namespace ts3toolkit::audio {

AudioEngine::AudioEngine(std::size_t capture_ring_capacity)
    : capture_(capture_ring_capacity),
      monitor_buffer_(core::kSampleRateHz / 2),
      playing_(false),
      overlay_(),
      overlay_position_(0) {}

AudioEngine::~AudioEngine() {
    stopMonitor();
    stopCapture();
}

void AudioEngine::monitorCallback(ma_device* device, void* output,
                                  const void*, ma_uint32 frame_count) {
    auto* self = static_cast<AudioEngine*>(device->pUserData);
    auto* samples = static_cast<core::Sample*>(output);
    const auto read = self->monitor_buffer_.read(samples, frame_count);
    std::fill(samples + read, samples + frame_count, 0);
}

bool AudioEngine::startMonitor() {
    if (monitor_running_) return true;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.sampleRate = core::kSampleRateHz;
    config.playback.format = ma_format_s16;
    config.playback.channels = core::kChannelCount;
    config.dataCallback = &AudioEngine::monitorCallback;
    config.pUserData = this;
    if (ma_device_init(nullptr, &config, &monitor_device_) != MA_SUCCESS) return false;
    if (ma_device_start(&monitor_device_) != MA_SUCCESS) {
        ma_device_uninit(&monitor_device_);
        return false;
    }
    monitor_running_ = true;
    return true;
}

void AudioEngine::stopMonitor() {
    {
        std::lock_guard<std::mutex> lock(overlay_mutex_);
        if (!monitor_running_) return;
        monitor_running_ = false;
    }
    ma_device_uninit(&monitor_device_);
    monitor_buffer_.reset();
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

    mixCapturedFrames(output, count, 1);
}

bool AudioEngine::mixCapturedFrames(core::Sample* output, std::size_t count,
                                    std::size_t channels) {
    if (!output || channels == 0 || !playing_.load(std::memory_order_acquire)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(overlay_mutex_);
    const std::size_t remaining = overlay_.size() - overlay_position_;
    if (remaining == 0) {
        playing_.store(false, std::memory_order_release);
        overlay_position_ = 0;
        return false;
    }

    const std::size_t to_mix = (std::min)(count, remaining);
    if (monitor_running_) {
        monitor_buffer_.write(overlay_.data() + overlay_position_, to_mix);
    }
    for (std::size_t i = 0; i < to_mix; ++i) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            core::mixAdd(output + i * channels + channel,
                         overlay_.data() + overlay_position_ + i, 1);
        }
    }
    overlay_position_ += to_mix;

    if (overlay_position_ >= overlay_.size()) {
        playing_.store(false, std::memory_order_release);
        overlay_position_ = 0;
    }
    return to_mix != 0;
}

}  // namespace ts3toolkit::audio
