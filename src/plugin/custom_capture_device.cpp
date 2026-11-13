#include "ts3toolkit/plugin/custom_capture_device.h"

#include <chrono>

namespace ts3toolkit::plugin {

CustomCaptureDevice::CustomCaptureDevice(ClientApi& client,
                                         audio::AudioEngine& engine,
                                         std::string device_id,
                                         std::string display_name)
    : client_(client),
      engine_(engine),
      device_id_(std::move(device_id)),
      display_name_(std::move(display_name)),
      frame_buffer_(core::kFrameSamples, 0),
      running_(false),
      stop_requested_(false) {}

CustomCaptureDevice::~CustomCaptureDevice() {
    stop();
}

bool CustomCaptureDevice::start() {
    if (running_.load()) {
        return true;
    }
    stop_requested_.store(false);

    if (!client_.registerCustomDevice(device_id_, display_name_,
                                      core::kSampleRateHz,
                                      core::kChannelCount)) {
        return false;
    }
    if (!engine_.startCapture()) {
        client_.unregisterCustomDevice(device_id_);
        return false;
    }

    running_.store(true);
    pump_thread_ = std::thread(&CustomCaptureDevice::runLoop, this);
    return true;
}

void CustomCaptureDevice::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_.store(true);
    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
    }
    wait_cv_.notify_all();
    if (pump_thread_.joinable()) {
        pump_thread_.join();
    }

    engine_.stopCapture();
    client_.unregisterCustomDevice(device_id_);
    running_.store(false);
}

bool CustomCaptureDevice::running() const {
    return running_.load();
}

void CustomCaptureDevice::setFrameHook(std::function<void()> hook) {
    frame_hook_ = std::move(hook);
}

void CustomCaptureDevice::pumpOnce() {
    engine_.fillFrame(frame_buffer_.data(), frame_buffer_.size());
    if (!client_.pushCaptureFrames(device_id_, frame_buffer_.data(),
                                   static_cast<int>(frame_buffer_.size()))) {
        // Fail closed: stop the pump on a client error rather than spinning on
        // repeated failures. The microphone remains usable for reconnection.
        stop_requested_.store(true);
    }
}

void CustomCaptureDevice::runLoop() {
    const auto frame_duration =
        std::chrono::milliseconds(core::kFrameDurationMs);
    auto next_deadline = std::chrono::steady_clock::now();

    while (!stop_requested_.load()) {
        if (frame_hook_) {
            frame_hook_();
        }
        pumpOnce();

        next_deadline += frame_duration;
        std::unique_lock<std::mutex> lock(wait_mutex_);
        wait_cv_.wait_until(lock, next_deadline,
                            [this] { return stop_requested_.load(); });
    }
}

}  // namespace ts3toolkit::plugin
