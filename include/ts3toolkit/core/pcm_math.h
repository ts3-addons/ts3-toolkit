#pragma once

#include "ts3toolkit/core/types.h"

namespace ts3toolkit::core {

// Reduce a wider integer into the valid sample range without overflow.
constexpr Sample clampToSample(int value) noexcept {
    if (value < kSampleMin) {
        return static_cast<Sample>(kSampleMin);
    }
    if (value > kSampleMax) {
        return static_cast<Sample>(kSampleMax);
    }
    return static_cast<Sample>(value);
}

// Apply a linear gain in [0.0, 1.0] to one sample using a wide intermediate.
constexpr Sample scaleSample(Sample input, float gain) noexcept {
    const int scaled = static_cast<int>(static_cast<float>(input) * gain);
    return clampToSample(scaled);
}

}  // namespace ts3toolkit::core
