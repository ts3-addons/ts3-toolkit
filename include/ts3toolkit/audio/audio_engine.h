#pragma once

#include "ts3toolkit/audio/decoded_sound.h"
#include "ts3toolkit/audio/microphone_capture.h"
#include "ts3toolkit/core/types.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>

namespace ts3toolkit::audio {

// Pump-side state of the entrance sound overlay.
enum class PlaybackState {
    kIdle,
    kPlaying,
};

// Owns microphone capture and the entrance sound overlay, and produces one
// mixed frame at a time on the audio thread. The microphone is always passed
// through; the overlay only changes data while playing.
class AudioEngine {
public:
    explicit AudioEngine(std::size_t capture_ring_capacity);
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool startCapture();
    void stopCapture();
    bool captureRunning() const;

    // Start overlaying a sound on the next frame, replacing any current one.
    // The volume is baked into the prepared overlay so the pump stays integer.
    void playSound(const DecodedSound& sound, float volume);
    void stopSound();

    PlaybackState state() const;

    // Called from the TS3 custom device callback for one frame. The output is
    // always fully written: microphone passthrough, zero-filled on underrun.
    void fillFrame(core::Sample* output, std::size_t count);

private:
    MicrophoneCapture capture_;
    std::atomic<bool> playing_;

    // Overlay swap is short (vector move) so the real-time pump is never held
    // off for a full copy.
    std::mutex overlay_mutex_;
    std::vector<core::Sample> overlay_;
    std::size_t overlay_position_;
};

}  // namespace ts3toolkit::audio
