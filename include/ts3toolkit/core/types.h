#pragma once

#include <cstddef>
#include <cstdint>

namespace ts3toolkit::core {

// Single PCM sample on the custom capture device: 16-bit signed little-endian.
using Sample = std::int16_t;

// Custom capture device format and fixed pump cadence.
inline constexpr int kSampleRateHz = 48000;
inline constexpr int kChannelCount = 1;
inline constexpr int kFrameDurationMs = 10;
inline constexpr int kFrameSamples =
    kSampleRateHz * kFrameDurationMs / 1000;  // 480 samples per 10 ms frame

// Inclusive range of a 16-bit signed sample.
inline constexpr int kSampleMin = -32768;
inline constexpr int kSampleMax = 32767;

static_assert(kFrameSamples == 480,
              "pump frame must hold exactly 480 mono samples at 48 kHz");

}  // namespace ts3toolkit::core
