#pragma once

#include <cstdint>

namespace ts3toolkit::platform {

// Monotonic time source used for scheduling and cooldown. Injected so logic can
// be tested with a deterministic fake clock.
class Clock {
public:
    virtual ~Clock() = default;

    // Monotonic millisecond timestamp; the epoch is implementation-defined.
    virtual std::int64_t nowMilliseconds() const = 0;
};

}  // namespace ts3toolkit::platform
