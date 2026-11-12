#include "ts3toolkit/entrance/entrance_controller.h"

namespace ts3toolkit::entrance {

namespace {
constexpr std::int64_t kMillisecondsPerSecond = 1000;
}  // namespace

EntranceController::EntranceController(const platform::Clock& clock,
                                       Playback& playback)
    : clock_(clock),
      playback_(playback),
      settings_(),
      prepared_sound_(nullptr),
      state_(ControllerState::kIdle),
      cooldown_start_ms_(0) {}

void EntranceController::updateSettings(
    const config::EntranceSoundSettings& settings,
    const audio::DecodedSound* prepared_sound) {
    settings_ = settings;
    prepared_sound_ = prepared_sound;

    // Disabling the feature while playing stops the overlay immediately.
    if (!settings_.enabled && state_ == ControllerState::kPlaying) {
        playback_.stopOverlay();
        state_ = ControllerState::kIdle;
    }
}

void EntranceController::onLocalClientEnteredChannel() {
    // Events during cooldown are dropped per the behavior contract.
    if (state_ == ControllerState::kCooldown) {
        return;
    }
    if (!settings_.enabled || settings_.sound_file.empty()) {
        return;
    }
    // Fail closed: without usable decoded data the microphone is left untouched.
    if (prepared_sound_ == nullptr || prepared_sound_->empty()) {
        return;
    }

    playback_.startOverlay(*prepared_sound_, settings_.volume);
    state_ = ControllerState::kPlaying;
}

void EntranceController::poll() {
    const std::int64_t now = clock_.nowMilliseconds();

    if (state_ == ControllerState::kPlaying &&
        !playback_.isOverlayPlaying()) {
        state_ = ControllerState::kCooldown;
        cooldown_start_ms_ = now;
    }

    if (state_ == ControllerState::kCooldown) {
        const std::int64_t window_ms =
            static_cast<std::int64_t>(settings_.cooldown_seconds) *
            kMillisecondsPerSecond;
        if (now - cooldown_start_ms_ >= window_ms) {
            state_ = ControllerState::kIdle;
        }
    }
}

ControllerState EntranceController::state() const {
    return state_;
}

}  // namespace ts3toolkit::entrance
