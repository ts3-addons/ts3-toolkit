#include "ts3toolkit/audio/microphone_capture.h"

namespace ts3toolkit::audio {

void MicrophoneCapture::dataCallback(ma_device* device, void* /*output*/,
                                     const void* input,
                                     ma_uint32 frame_count) {
    if (input == nullptr) {
        return;
    }
    auto* self = static_cast<MicrophoneCapture*>(device->pUserData);
    const auto* samples = static_cast<const core::Sample*>(input);
    // Real-time source: best effort write, newest data is dropped if the ring
    // is momentarily full so the capture thread never blocks.
    self->ring_.write(samples, frame_count);
}

MicrophoneCapture::MicrophoneCapture(std::size_t ring_capacity)
    : device_{},
      initialized_(false),
      running_(false),
      ring_(ring_capacity) {}

MicrophoneCapture::~MicrophoneCapture() {
    stop();
}

bool MicrophoneCapture::start() {
    if (running_) {
        return true;
    }

    ma_device_config config =
        ma_device_config_init(ma_device_type_capture);
    config.sampleRate = core::kSampleRateHz;
    config.capture.format = ma_format_s16;
    config.capture.channels = core::kChannelCount;
    config.dataCallback = &MicrophoneCapture::dataCallback;
    config.pUserData = this;

    if (ma_device_init(nullptr, &config, &device_) != MA_SUCCESS) {
        initialized_ = false;
        return false;
    }
    initialized_ = true;

    if (ma_device_start(&device_) != MA_SUCCESS) {
        ma_device_uninit(&device_);
        initialized_ = false;
        return false;
    }
    running_ = true;
    return true;
}

void MicrophoneCapture::stop() {
    if (running_) {
        ma_device_stop(&device_);
        running_ = false;
    }
    if (initialized_) {
        ma_device_uninit(&device_);
        initialized_ = false;
    }
}

bool MicrophoneCapture::isRunning() const {
    return running_;
}

std::size_t MicrophoneCapture::read(core::Sample* dst, std::size_t count) {
    return ring_.read(dst, count);
}

}  // namespace ts3toolkit::audio
