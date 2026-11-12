#pragma once

#include "ts3toolkit/core/ring_buffer.h"
#include "ts3toolkit/core/types.h"

#include <miniaudio.h>

#include <cstddef>

namespace ts3toolkit::audio {

// Captures the physical microphone with miniaudio and streams s16 samples into
// an SPSC ring buffer for the audio pump. If the device cannot be opened the
// failure is reported and the pump keeps running with silence instead.
class MicrophoneCapture {
public:
    explicit MicrophoneCapture(std::size_t ring_capacity);
    ~MicrophoneCapture();

    MicrophoneCapture(const MicrophoneCapture&) = delete;
    MicrophoneCapture& operator=(const MicrophoneCapture&) = delete;

    // Open and start the default capture device.
    bool start();
    void stop();
    bool isRunning() const;

    // Reader side used by the pump; returns the number of samples dequeued.
    std::size_t read(core::Sample* dst, std::size_t count);

private:
    static void dataCallback(ma_device* device, void* output,
                             const void* input, ma_uint32 frame_count);

    ma_device device_;
    bool initialized_;
    bool running_;
    core::RingBuffer ring_;
};

}  // namespace ts3toolkit::audio
