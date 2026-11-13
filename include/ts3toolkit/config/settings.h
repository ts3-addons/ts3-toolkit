#pragma once

#include <string>

namespace ts3toolkit::config {

inline constexpr int kCurrentSchemaVersion = 2;

// Validation bounds. They are part of the settings contract and are shared by
// the parser, the storage layer, and the future settings UI.
inline constexpr float kMinVolume = 0.0f;
inline constexpr float kMaxVolume = 1.0f;
inline constexpr int kMinCooldownSeconds = 0;
inline constexpr int kMaxCooldownSeconds = 300;
inline constexpr int kMaxDelayMs = 30000;
inline constexpr std::size_t kMaxSoundPathLength = 1024;

// Validated entrance sound settings. Field meanings match
// docs/entrance-sound.md.
struct EntranceSoundSettings {
    bool enabled = true;
    std::string sound_file;  // empty means no sound is configured
    float volume = 0.8f;     // [0.0, 1.0]
    int cooldown_seconds = 5;
    bool local_monitor = false;
    int delay_ms = 0;
};

// Full persisted settings document.
struct SettingsDocument {
    int schema_version = kCurrentSchemaVersion;
    EntranceSoundSettings entrance_sound;
};

// Outcome of validating one settings block.
struct ValidationResult {
    bool valid = true;
    std::string field;   // name of the first invalid field when valid == false
    std::string reason;
};

ValidationResult validate(const EntranceSoundSettings& settings);

}  // namespace ts3toolkit::config
