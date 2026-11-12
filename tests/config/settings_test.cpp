#include <string>

#include "tests/test_macros.h"
#include "ts3toolkit/config/settings.h"

using namespace ts3toolkit::config;

void runTests() {
    EntranceSoundSettings defaults;
    CHECK_TRUE(validate(defaults).valid);

    EntranceSoundSettings bad_volume = defaults;
    bad_volume.volume = 1.5f;
    ValidationResult r1 = validate(bad_volume);
    CHECK_TRUE(!r1.valid);
    CHECK_EQ(r1.field, std::string("volume"));

    bad_volume.volume = -0.1f;
    CHECK_TRUE(!validate(bad_volume).valid);

    EntranceSoundSettings bad_cooldown = defaults;
    bad_cooldown.cooldown_seconds = kMaxCooldownSeconds + 1;
    ValidationResult r2 = validate(bad_cooldown);
    CHECK_TRUE(!r2.valid);
    CHECK_EQ(r2.field, std::string("cooldown_seconds"));

    bad_cooldown.cooldown_seconds = -1;
    CHECK_TRUE(!validate(bad_cooldown).valid);

    EntranceSoundSettings edges = defaults;
    edges.volume = kMinVolume;
    edges.cooldown_seconds = kMinCooldownSeconds;
    CHECK_TRUE(validate(edges).valid);
    edges.volume = kMaxVolume;
    edges.cooldown_seconds = kMaxCooldownSeconds;
    CHECK_TRUE(validate(edges).valid);

    EntranceSoundSettings long_path = defaults;
    long_path.sound_file.assign(kMaxSoundPathLength + 1, 'a');
    CHECK_TRUE(!validate(long_path).valid);
}

TEST_MAIN()
