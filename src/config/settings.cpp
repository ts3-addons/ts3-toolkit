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
    return {true, "", ""};
}

}  // namespace ts3toolkit::config
