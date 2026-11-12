#pragma once

#include <cmath>
#include <cstdio>

namespace ts3toolkit::testing {

// Process-wide counter of failed assertions; a non-zero value fails the test.
inline int& failureCount() {
    static int failures = 0;
    return failures;
}

}  // namespace ts3toolkit::testing

#define CHECK_TRUE(condition)                                              \
    do {                                                                   \
        if (!(condition)) {                                                \
            std::fprintf(stderr, "CHECK_TRUE failed: %s (%s:%d)\n",        \
                         #condition, __FILE__, __LINE__);                  \
            ++ts3toolkit::testing::failureCount();                        \
        }                                                                  \
    } while (0)

#define CHECK_EQ(actual, expected)                                         \
    do {                                                                   \
        auto check_actual = (actual);                                      \
        auto check_expected = (expected);                                  \
        if (!(check_actual == check_expected)) {                           \
            std::fprintf(stderr,                                           \
                         "CHECK_EQ failed: %s == %s, got %s vs %s (%s:%d)\n", \
                         #actual, #expected, #check_actual, #check_expected, \
                         __FILE__, __LINE__);                              \
            ++ts3toolkit::testing::failureCount();                        \
        }                                                                  \
    } while (0)

#define CHECK_CLOSE(actual, expected, tolerance)                           \
    do {                                                                   \
        const double check_diff = std::fabs(static_cast<double>(actual) -  \
                                            static_cast<double>(expected)); \
        if (!(check_diff <= static_cast<double>(tolerance))) {             \
            std::fprintf(stderr, "CHECK_CLOSE failed: %s ~ %s (%s:%d)\n",  \
                         #actual, #expected, __FILE__, __LINE__);          \
            ++ts3toolkit::testing::failureCount();                        \
        }                                                                  \
    } while (0)

#define TEST_MAIN()                                                         \
    int main()                                                              \
    {                                                                       \
        runTests();                                                         \
        if (ts3toolkit::testing::failureCount() != 0) {                     \
            std::fprintf(stderr, "%d check(s) failed\n",                    \
                         ts3toolkit::testing::failureCount());              \
            return 1;                                                       \
        }                                                                   \
        return 0;                                                           \
    }
