#pragma once

#include "ts3toolkit/platform/clock.h"

namespace ts3toolkit::platform {

// Clock backed by std::chrono::steady_clock; never moves backwards.
class SystemClock : public Clock {
public:
    std::int64_t nowMilliseconds() const override;
};

}  // namespace ts3toolkit::platform
