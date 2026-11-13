#pragma once

#include "ts3toolkit/audio/audio_engine.h"
#include "ts3toolkit/core/types.h"
#include "ts3toolkit/plugin/client_api.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ts3toolkit::plugin {

// Owns the registered custom capture device and the real-time pump thread.
// Every iteration produces one mixed frame from the AudioEngine and pushes it
// into the client. The device fails closed: a push error halts the pump safely
// so the user can reconnect, without ever blocking the microphone path.
class CustomCaptureDevice {
public:
    CustomCaptureDevice(ClientApi& client, audio::AudioEngine& engine,
                        std::string device_id, std::string display_name);
    ~CustomCaptureDevice();

    CustomCaptureDevice(const CustomCaptureDevice&) = delete;
    CustomCaptureDevice& operator=(const CustomCaptureDevice&) = delete;

    bool start();
    void stop();
    bool running() const;

    // Perform exactly one pump iteration. Exposed so unit tests can verify the
    // data flow deterministically without relying on thread timing.
    void pumpOnce();

    // Optional hook invoked at the start of every pump iteration. Used by the
    // runtime to advance the feature controller on the same fixed cadence.
    void setFrameHook(std::function<void()> hook);
    void setFailureHook(std::function<void()> hook);

private:
    void runLoop();

    ClientApi& client_;
    audio::AudioEngine& engine_;
    std::string device_id_;
    std::string display_name_;

    std::vector<core::Sample> frame_buffer_;
    std::thread pump_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> stop_requested_;

    // Wakeable wait so stop() returns promptly instead of waiting a full frame.
    std::mutex wait_mutex_;
    std::condition_variable wait_cv_;

    std::function<void()> frame_hook_;
    std::function<void()> failure_hook_;
};

}  // namespace ts3toolkit::plugin
