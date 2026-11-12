#pragma once

#include "ts3toolkit/core/types.h"

#include <cstddef>

namespace ts3toolkit::core {

// Mix an overlay into dst in place: dst[i] = clamp(dst[i] + overlay[i]).
// The two buffers must hold at least count samples.
void mixAdd(Sample* dst, const Sample* overlay, std::size_t count) noexcept;

// Scale every sample in place by a gain in [0.0, 1.0].
void applyGain(Sample* buffer, std::size_t count, float gain) noexcept;

}  // namespace ts3toolkit::core
