#include <array>

#include "tests/test_macros.h"
#include "ts3toolkit/core/ring_buffer.h"

using namespace ts3toolkit::core;

void runTests() {
    RingBuffer rb(8);
    CHECK_EQ(rb.capacity(), 8u);
    CHECK_TRUE(rb.isEmpty());

    std::array<Sample, 5> out{};
    CHECK_EQ(rb.read(out.data(), out.size()), 0u);  // empty read

    const std::array<Sample, 5> first{1, 2, 3, 4, 5};
    CHECK_EQ(rb.write(first.data(), first.size()), 5u);
    CHECK_EQ(rb.size(), 5u);

    const std::array<Sample, 5> second{6, 7, 8, 9, 10};
    CHECK_EQ(rb.write(second.data(), second.size()), 3u);  // 3 free slots
    CHECK_TRUE(rb.isFull());
    CHECK_EQ(rb.write(second.data(), 1), 0u);

    CHECK_EQ(rb.read(out.data(), out.size()), 5u);
    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(out[i], first[i]);
    }
    CHECK_EQ(rb.size(), 3u);

    std::array<Sample, 3> rest{};
    CHECK_EQ(rb.read(rest.data(), rest.size()), 3u);
    CHECK_EQ(rest[0], 6);
    CHECK_EQ(rest[1], 7);
    CHECK_EQ(rest[2], 8);
    CHECK_TRUE(rb.isEmpty());

    // Repeated cycles exercise the wrap-around mask.
    for (int cycle = 0; cycle < 10; ++cycle) {
        std::array<Sample, 8> block{};
        for (int i = 0; i < 8; ++i) {
            block[i] = static_cast<Sample>(cycle * 8 + i);
        }
        CHECK_EQ(rb.write(block.data(), block.size()), 8u);
        std::array<Sample, 8> got{};
        CHECK_EQ(rb.read(got.data(), got.size()), 8u);
        for (int i = 0; i < 8; ++i) {
            CHECK_EQ(got[i], block[i]);
        }
    }

    // Non-power-of-two request is rounded up.
    RingBuffer rounded(5);
    CHECK_EQ(rounded.capacity(), 8u);

    rb.reset();
    CHECK_TRUE(rb.isEmpty());
}

TEST_MAIN()
