#pragma once

#include "ts3toolkit/core/types.h"

#include <cstddef>
#include <vector>

namespace ts3toolkit::audio {

// A decoded entrance sound normalized to the custom capture format:
// 48 kHz mono 16-bit PCM.
struct DecodedSound {
    std::vector<core::Sample> samples;

    bool empty() const {
        return samples.empty();
    }

    std::size_t lengthSamples() const {
        return samples.size();
    }
};

}  // namespace ts3toolkit::audio
