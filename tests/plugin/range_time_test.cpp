#include <string>

#include "src/plugin/range_time.h"
#include "tests/test_macros.h"

using ts3toolkit::plugin::formatRangeTime;
using ts3toolkit::plugin::parseRangeTime;

void runTests() {
    CHECK_EQ(formatRangeTime(0), std::wstring(L"00:00.000"));
    CHECK_EQ(formatRangeTime(65250), std::wstring(L"01:05.250"));
    CHECK_EQ(formatRangeTime(300000), std::wstring(L"05:00.000"));

    int result = -1;
    CHECK_TRUE(parseRangeTime(L"1:05.25", result));
    CHECK_EQ(result, 65250);
    CHECK_TRUE(parseRangeTime(L"00:05.2", result));
    CHECK_EQ(result, 5200);
    CHECK_TRUE(parseRangeTime(L"00:05", result));
    CHECK_EQ(result, 5000);
    CHECK_TRUE(parseRangeTime(L"00:00.001", result));
    CHECK_EQ(result, 1);

    for (int value = 0; value <= 300000; ++value) {
        CHECK_TRUE(parseRangeTime(formatRangeTime(value), result));
        CHECK_EQ(result, value);
    }

    for (const auto* value : {L"", L"1234", L"00:60", L"05:00.001", L"00:01.",
                              L"00:01.0000", L"-1:00", L"00:ab", L"00:01.2x"}) {
        result = 42;
        CHECK_TRUE(!parseRangeTime(value, result));
        CHECK_EQ(result, 42);
    }
}

TEST_MAIN()
