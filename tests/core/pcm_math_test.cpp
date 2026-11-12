#include "tests/test_macros.h"

#include "ts3toolkit/core/pcm_math.h"
#include "ts3toolkit/core/types.h"

using namespace ts3toolkit::core;

void runTests() {
    CHECK_EQ(clampToSample(0), 0);
    CHECK_EQ(clampToSample(40000), kSampleMax);
    CHECK_EQ(clampToSample(-40000), kSampleMin);
    CHECK_EQ(clampToSample(kSampleMax), kSampleMax);
    CHECK_EQ(clampToSample(kSampleMin), kSampleMin);

    CHECK_EQ(scaleSample(1000, 0.0f), 0);
    CHECK_EQ(scaleSample(1000, 1.0f), 1000);
    CHECK_EQ(scaleSample(-1000, 0.5f), -500);
}

TEST_MAIN()
