#include <array>

#include "tests/test_macros.h"
#include "ts3toolkit/audio/audio_engine.h"
#include "ts3toolkit/audio/decoded_sound.h"

using namespace ts3toolkit::audio;

namespace {

void allZero(const std::array<ts3toolkit::core::Sample, 4>& frame) {
    for (auto sample : frame) {
        CHECK_EQ(sample, 0);
    }
}

}  // namespace

void runTests() {
    AudioEngine engine(64);
    CHECK_EQ(engine.state(), PlaybackState::kIdle);

    // With no capture running, frames are defined silence.
    std::array<ts3toolkit::core::Sample, 4> frame{};
    engine.fillFrame(frame.data(), frame.size());
    allZero(frame);

    // Six-sample sound consumed across 4-sample frames at full volume.
    DecodedSound sound;
    sound.samples = {100, 200, 300, 400, 500, 600};
    engine.playSound(sound, 1.0f);
    CHECK_EQ(engine.state(), PlaybackState::kPlaying);

    std::array<ts3toolkit::core::Sample, 4> first{};
    engine.fillFrame(first.data(), first.size());
    CHECK_EQ(first[0], 100);
    CHECK_EQ(first[1], 200);
    CHECK_EQ(first[2], 300);
    CHECK_EQ(first[3], 400);
    CHECK_EQ(engine.state(), PlaybackState::kPlaying);

    std::array<ts3toolkit::core::Sample, 4> second{};
    engine.fillFrame(second.data(), second.size());
    CHECK_EQ(second[0], 500);
    CHECK_EQ(second[1], 600);
    CHECK_EQ(second[2], 0);
    CHECK_EQ(second[3], 0);
    CHECK_EQ(engine.state(), PlaybackState::kIdle);

    // Half volume scales the first frame.
    engine.playSound(sound, 0.5f);
    std::array<ts3toolkit::core::Sample, 2> half{};
    engine.fillFrame(half.data(), half.size());
    CHECK_EQ(half[0], 50);
    CHECK_EQ(half[1], 100);

    // The same gain stage is used for transmitted audio and local preview.
    engine.playSound(sound, 0.0f);
    std::array<ts3toolkit::core::Sample, 4> muted{};
    engine.fillFrame(muted.data(), muted.size());
    allZero(muted);

    // stopSound silences subsequent frames.
    engine.stopSound();
    CHECK_EQ(engine.state(), PlaybackState::kIdle);
    std::array<ts3toolkit::core::Sample, 4> after_stop{};
    engine.fillFrame(after_stop.data(), after_stop.size());
    allZero(after_stop);

    // Replacing a playing sound resets the cursor.
    engine.playSound(sound, 1.0f);
    std::array<ts3toolkit::core::Sample, 4> skipped{};
    engine.fillFrame(skipped.data(), skipped.size());

    DecodedSound other;
    other.samples = {10, 20};
    engine.playSound(other, 1.0f);
    std::array<ts3toolkit::core::Sample, 4> replaced{};
    engine.fillFrame(replaced.data(), replaced.size());
    CHECK_EQ(replaced[0], 10);
    CHECK_EQ(replaced[1], 20);
    CHECK_EQ(replaced[2], 0);
    CHECK_EQ(replaced[3], 0);
    CHECK_EQ(engine.state(), PlaybackState::kIdle);
}

TEST_MAIN()
