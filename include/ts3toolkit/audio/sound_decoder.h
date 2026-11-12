#pragma once

#include "ts3toolkit/audio/decoded_sound.h"

#include <string>

namespace ts3toolkit::audio {

// Maximum entrance sound duration, matching docs/entrance-sound.md.
inline constexpr int kMaxEntranceSoundSeconds = 30;
inline constexpr std::size_t kMaxEntranceSoundSamples =
    core::kSampleRateHz * kMaxEntranceSoundSeconds;

// Category of a decode attempt.
enum class DecodeStatus {
    kOk,
    kFileNotFound,
    kUnsupportedFormat,
    kTooLong,
    kDecodeFailed,
};

struct DecodeResult {
    DecodedSound sound;
    DecodeStatus status;
};

// Decodes supported sound files into normalized 48 kHz mono s16 PCM.
class SoundDecoder {
public:
    DecodeResult decodeFile(const std::string& path) const;
};

}  // namespace ts3toolkit::audio
