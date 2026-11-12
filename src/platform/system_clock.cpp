#include "ts3toolkit/platform/system_clock.h"

#include <chrono>

namespace ts3toolkit::platform {

std::int64_t SystemClock::nowMilliseconds() const {
    using steady = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               steady::now().time_since_epoch())
        .count();
}

}  // namespace ts3toolkit::platform
