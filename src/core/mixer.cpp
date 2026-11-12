#include "ts3toolkit/core/mixer.h"

#include "ts3toolkit/core/pcm_math.h"

namespace ts3toolkit::core {

void mixAdd(Sample* dst, const Sample* overlay, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        // Wide intermediate prevents signed 16-bit overflow before clamping.
        dst[i] = clampToSample(static_cast<int>(dst[i]) +
                               static_cast<int>(overlay[i]));
    }
}

void applyGain(Sample* buffer, std::size_t count, float gain) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        buffer[i] = scaleSample(buffer[i], gain);
    }
}

}  // namespace ts3toolkit::core
