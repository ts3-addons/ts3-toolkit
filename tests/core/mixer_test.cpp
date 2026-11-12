#include <array>

#include "tests/test_macros.h"
#include "ts3toolkit/core/mixer.h"
#include "ts3toolkit/core/types.h"

using namespace ts3toolkit::core;

void runTests() {
    // Plain addition without clipping.
    std::array<Sample, 4> dst{10, -20, 0, 100};
    const std::array<Sample, 4> overlay{5, 20, 7, -50};
    mixAdd(dst.data(), overlay.data(), dst.size());
    CHECK_EQ(dst[0], 15);
    CHECK_EQ(dst[1], 0);
    CHECK_EQ(dst[2], 7);
    CHECK_EQ(dst[3], 50);

    // Positive clipping.
    std::array<Sample, 2> hi{30000, 32767};
    const std::array<Sample, 2> hi_overlay{10000, 1};
    mixAdd(hi.data(), hi_overlay.data(), hi.size());
    CHECK_EQ(hi[0], kSampleMax);
    CHECK_EQ(hi[1], kSampleMax);

    // Negative clipping.
    std::array<Sample, 2> lo{-30000, -32768};
    const std::array<Sample, 2> lo_overlay{-10000, -1};
    mixAdd(lo.data(), lo_overlay.data(), lo.size());
    CHECK_EQ(lo[0], kSampleMin);
    CHECK_EQ(lo[1], kSampleMin);

    // Gain: zero mutes, identity preserves, midpoint scales.
    std::array<Sample, 3> muted{1000, -1000, 500};
    applyGain(muted.data(), muted.size(), 0.0f);
    CHECK_EQ(muted[0], 0);
    CHECK_EQ(muted[1], 0);
    CHECK_EQ(muted[2], 0);

    std::array<Sample, 3> identity{1000, -1000, 500};
    applyGain(identity.data(), identity.size(), 1.0f);
    CHECK_EQ(identity[0], 1000);
    CHECK_EQ(identity[1], -1000);
    CHECK_EQ(identity[2], 500);

    std::array<Sample, 1> half{1000};
    applyGain(half.data(), half.size(), 0.5f);
    CHECK_EQ(half[0], 500);
}

TEST_MAIN()
