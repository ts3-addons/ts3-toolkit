#include "ts3toolkit/config/settings.h"

namespace ts3toolkit::config {

ValidationResult validate(const EntranceSoundSettings& settings) {
    if (!(settings.volume >= kMinVolume && settings.volume <= kMaxVolume)) {
        return {false, "volume", "value must be within [0.0, 1.0]"};
    }
    if (settings.cooldown_seconds < kMinCooldownSeconds ||
        settings.cooldown_seconds > kMaxCooldownSeconds) {
        return {false, "cooldown_seconds",
                "value must be within [0, 300]"};
    }
    if (settings.sound_file.size() > kMaxSoundPathLength) {
        return {false, "sound_file", "path exceeds the maximum length"};
    }
    if (settings.delay_ms < 0 || settings.delay_ms > kMaxDelayMs) {
        return {false, "delay_ms", "value must be within [0, 30000]"};
    }
    if (settings.start_ms < 0 || settings.start_ms > kMaxSourceDurationMs ||
        settings.end_ms < -1 || settings.end_ms > kMaxSourceDurationMs ||
        (settings.end_ms != -1 &&
         (settings.end_ms <= settings.start_ms ||
          settings.end_ms - settings.start_ms > kMaxSelectionDurationMs))) {
        return {false, "range", "invalid sound selection"};
    }
    return {true, "", ""};
}

}  // namespace ts3toolkit::config
