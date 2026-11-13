#include "tests/test_macros.h"
#include "ts3toolkit/audio/decoded_sound.h"
#include "ts3toolkit/config/settings.h"
#include "ts3toolkit/entrance/entrance_controller.h"

using namespace ts3toolkit::entrance;

namespace {

class FakeClock : public ts3toolkit::platform::Clock {
public:
    std::int64_t value = 0;

    std::int64_t nowMilliseconds() const override {
        return value;
    }

    void advance(std::int64_t milliseconds) {
        value += milliseconds;
    }
};

class FakePlayback : public EntranceController::Playback {
public:
    bool playing = false;
    int start_count = 0;
    int stop_count = 0;
    float last_volume = 0.0f;

    void startOverlay(const ts3toolkit::audio::DecodedSound&,
                      float volume) override {
        playing = true;
        ++start_count;
        last_volume = volume;
    }

    void stopOverlay() override {
        playing = false;
        ++stop_count;
    }

    bool isOverlayPlaying() const override {
        return playing;
    }
};

}  // namespace

void runTests() {
    FakeClock clock;
    FakePlayback playback;
    EntranceController controller(clock, playback);

    ts3toolkit::config::EntranceSoundSettings settings;  // defaults
    // A configured, non-empty file path is required to trigger per the
    // behavior contract; the no_path case below clears it on purpose.
    settings.sound_file = "entrance.wav";
    ts3toolkit::audio::DecodedSound sound;
    sound.samples = {1, 2, 3, 4};

    // No prepared sound yet: fail closed.
    controller.updateSettings(settings, nullptr);
    controller.onLocalClientEnteredChannel();
    CHECK_EQ(controller.state(), ControllerState::kIdle);

    FakeClock delay_clock;
    FakePlayback delay_playback;
    EntranceController delayed(delay_clock, delay_playback);
    settings.delay_ms = 1500;
    delayed.updateSettings(settings, &sound);
    delayed.onLocalClientEnteredChannel();
    CHECK_EQ(delayed.state(), ControllerState::kPending);
    delayed.onLocalClientEnteredChannel();
    delay_clock.advance(1499);
    delayed.poll();
    CHECK_EQ(delay_playback.start_count, 0);
    delay_clock.advance(1);
    delayed.poll();
    CHECK_EQ(delay_playback.start_count, 1);
    CHECK_EQ(delayed.state(), ControllerState::kPlaying);

    FakePlayback cancel_playback;
    EntranceController cancelled(delay_clock, cancel_playback);
    cancelled.updateSettings(settings, &sound);
    cancelled.onLocalClientEnteredChannel();
    settings.enabled = false;
    cancelled.updateSettings(settings, &sound);
    delay_clock.advance(1500);
    cancelled.poll();
    CHECK_EQ(cancel_playback.start_count, 0);
    settings.enabled = true;
    cancelled.updateSettings(settings, &sound);
    cancelled.onLocalClientEnteredChannel();
    cancelled.cancelPending();
    delay_clock.advance(1500);
    cancelled.poll();
    CHECK_EQ(cancel_playback.start_count, 0);
    settings.enabled = true;
    settings.delay_ms = 0;
    CHECK_EQ(playback.start_count, 0);

    // With prepared data the trigger plays.
    controller.updateSettings(settings, &sound);
    controller.onLocalClientEnteredChannel();
    CHECK_EQ(controller.state(), ControllerState::kPlaying);
    CHECK_EQ(playback.start_count, 1);
    CHECK_CLOSE(playback.last_volume, 0.8f, 0.0001);

    // Stays playing while the engine is active.
    controller.poll();
    CHECK_EQ(controller.state(), ControllerState::kPlaying);

    // Engine finishing moves to cooldown.
    playback.playing = false;
    controller.poll();
    CHECK_EQ(controller.state(), ControllerState::kCooldown);

    // Triggers during cooldown are dropped.
    controller.onLocalClientEnteredChannel();
    CHECK_EQ(playback.start_count, 1);

    // The window boundary is honored.
    clock.advance(4999);
    controller.poll();
    CHECK_EQ(controller.state(), ControllerState::kCooldown);
    clock.advance(1);
    controller.poll();
    CHECK_EQ(controller.state(), ControllerState::kIdle);

    // A new trigger is accepted after cooldown.
    controller.onLocalClientEnteredChannel();
    CHECK_EQ(controller.state(), ControllerState::kPlaying);
    CHECK_EQ(playback.start_count, 2);

    // Disabling while playing stops the overlay.
    ts3toolkit::config::EntranceSoundSettings disabled = settings;
    disabled.enabled = false;
    controller.updateSettings(disabled, &sound);
    CHECK_EQ(controller.state(), ControllerState::kIdle);
    CHECK_EQ(playback.stop_count, 1);
    controller.onLocalClientEnteredChannel();
    CHECK_EQ(controller.state(), ControllerState::kIdle);

    // An empty sound file prevents playback.
    ts3toolkit::config::EntranceSoundSettings no_path = settings;
    no_path.sound_file = "";
    controller.updateSettings(no_path, &sound);
    controller.onLocalClientEnteredChannel();
    CHECK_EQ(controller.state(), ControllerState::kIdle);

    // Zero cooldown returns to idle on the next poll.
    ts3toolkit::config::EntranceSoundSettings zero_cooldown = settings;
    zero_cooldown.cooldown_seconds = 0;
    controller.updateSettings(zero_cooldown, &sound);
    controller.onLocalClientEnteredChannel();
    CHECK_EQ(controller.state(), ControllerState::kPlaying);
    playback.playing = false;
    controller.poll();
    CHECK_EQ(controller.state(), ControllerState::kIdle);
}

TEST_MAIN()
