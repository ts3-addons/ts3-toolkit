#include "ts3toolkit/plugin/plugin_runtime.h"

#include <chrono>
#include <filesystem>
#include <utility>

namespace ts3toolkit::plugin {

namespace fs = std::filesystem;

PluginRuntime::PluginRuntime(ClientApi& client)
    : client_(client),
      audio_engine_(kCaptureRingCapacity),
      clock_(),
      playback_adapter_(audio_engine_),
      controller_(clock_, playback_adapter_),
      sound_decoder_(),
      prepared_sound_(),
      talk_state_(client_),
      transmission_forced_(false),
      config_store_(nullptr),
      settings_doc_(),
      data_directory_(),
      initialized_(false) {}

PluginRuntime::~PluginRuntime() {
    shutdown();
}

bool PluginRuntime::initialize() {
    if (initialized_) {
        return true;
    }

    std::string plugins_root = client_.pluginPath();
    const std::string config_directory = client_.configPath();
    if (config_directory.empty()) {
        client_.log("client did not report config path; device not registered",
                    LogSeverity::kError);
        return false;
    }

    // getPluginPath returns the shared plugins directory; the plugin's private
    // data folder is named after the plugin binary base name.
    if (!plugins_root.empty()) {
        data_directory_ =
            (fs::u8path(plugins_root) / kPluginDataFolderName).u8string();
    }

    const fs::path config_file =
        fs::u8path(config_directory) / "ts3toolkit.ini";
    config_store_ =
        std::make_unique<config::ConfigStore>(config_file.u8string());

    config::LoadOutcome loaded = config_store_->load();
    settings_doc_ = std::move(loaded.document);
    if (loaded.status == config::LoadStatus::kRejected) {
        client_.log("settings file invalid; using defaults",
                    LogSeverity::kWarning);
    }

    // Decode before scheduling so the pump never performs file I/O.
    reloadSound(settings_doc_.entrance_sound);

    if (settings_doc_.entrance_sound.local_monitor && !audio_engine_.startMonitor()) {
        client_.log("local audio monitor failed to start", LogSeverity::kWarning);
    }

    stop_worker_.store(false, std::memory_order_release);
    worker_ = std::thread([this] {
        std::unique_lock<std::mutex> wait_lock(worker_mutex_);
        while (!stop_worker_.load(std::memory_order_acquire)) {
            wait_lock.unlock();
            if (connection_active_.load(std::memory_order_acquire)) {
                std::unique_lock<std::mutex> lock(controller_mutex_, std::try_to_lock);
                if (lock.owns_lock() && connection_active_.load(std::memory_order_acquire)) {
                    if (controller_.state() == entrance::ControllerState::kPending &&
                        client_.currentConnectionId() != controller_connection_id_) {
                        controller_.cancelPending();
                    }
                    controller_.poll();
                    syncTransmission();
                }
            }
            wait_lock.lock();
            worker_cv_.wait_for(wait_lock, std::chrono::milliseconds(10), [this] {
                return stop_worker_.load(std::memory_order_acquire);
            });
        }
    });

    initialized_ = true;
    return true;
}

void PluginRuntime::shutdown() {
    if (!initialized_) {
        return;
    }

    const bool was_connection_active =
        connection_active_.exchange(false, std::memory_order_acq_rel);
    stop_worker_.store(true, std::memory_order_release);
    worker_cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    audio_engine_.stopMonitor();

    // Restore the user's original talk state after the pump has stopped.
    if (transmission_forced_ && was_connection_active) {
        talk_state_.endForced(client_.currentConnectionId());
        transmission_forced_ = false;
    }
    talk_state_.discardForced();

    playback_adapter_.stopOverlay();
    config_store_.reset();
    prepared_sound_.samples.clear();
    data_directory_.clear();

    initialized_ = false;
}

void PluginRuntime::syncTransmission() {
    const bool playing = playback_adapter_.isOverlayPlaying();
    const uint64_t connection_id = controller_connection_id_;

    if (playing && !transmission_forced_) {
        if (talk_state_.beginForced(connection_id)) {
            transmission_forced_ = true;
        }
    } else if (!playing && transmission_forced_) {
        talk_state_.endForced(connection_id);
        transmission_forced_ = false;
    }
}

void PluginRuntime::onClientMove(uint64_t connection_id, uint16_t client_id,
                                 uint64_t new_channel_id) {
    if (!initialized_) {
        return;
    }

    uint16_t local_id = 0;
    if (!client_.getLocalClientId(connection_id, local_id)) {
        return;
    }
    if (client_id != local_id) {
        return;  // Movement of other clients is ignored.
    }
    if (new_channel_id == 0) {
        std::lock_guard<std::mutex> lock(controller_mutex_);
        controller_.cancelPending();
        return;  // Disconnects and leaves never trigger.
    }

    if (client_.currentConnectionId() != connection_id) {
        return;
    }

    // Enabled/configured/cooldown checks are owned by the controller.
    std::lock_guard<std::mutex> lock(controller_mutex_);
    controller_.cancelPending();
    if (controller_.state() == entrance::ControllerState::kIdle) {
        controller_connection_id_ = connection_id;
    }
    controller_.onLocalClientEnteredChannel();
}

void PluginRuntime::onConnectionEstablished(uint64_t connection_id) {
    if (!initialized_) {
        return;
    }

    uint16_t local_id = 0;
    if (!client_.getLocalClientId(connection_id, local_id)) {
        return;
    }
    uint64_t channel_id = 0;
    if (!client_.getChannelOfClient(connection_id, local_id, channel_id)) {
        return;
    }
    if (channel_id == 0) {
        return;
    }

    if (client_.currentConnectionId() != connection_id) {
        return;
    }

    active_connection_id_.store(connection_id, std::memory_order_release);
    connection_active_.store(true, std::memory_order_release);

    std::lock_guard<std::mutex> lock(controller_mutex_);
    controller_.cancelPending();
    if (controller_.state() == entrance::ControllerState::kIdle) {
        controller_connection_id_ = connection_id;
    }
    controller_.onLocalClientEnteredChannel();
}

void PluginRuntime::onConnectionDisconnected(uint64_t connection_id) {
    connection_active_.store(false, std::memory_order_release);
    std::lock_guard<std::mutex> lock(controller_mutex_);
    if (controller_connection_id_ == connection_id) controller_.cancelPending();
    playback_adapter_.stopOverlay();
    talk_state_.discardForced();
    transmission_forced_ = false;
}

bool PluginRuntime::onCapturedAudio(uint64_t connection_id, int16_t* samples,
                                    int frames, int channels) {
    if (!connection_active_.load(std::memory_order_acquire) ||
        active_connection_id_.load(std::memory_order_acquire) != connection_id ||
        !samples || frames <= 0 || channels <= 0 || channels > 2) {
        return false;
    }
    return audio_engine_.mixCapturedFrames(samples, static_cast<std::size_t>(frames),
                                           static_cast<std::size_t>(channels));
}

void PluginRuntime::reloadSound(
    const config::EntranceSoundSettings& settings) {
    prepared_sound_.samples.clear();

    if (!settings.sound_file.empty()) {
        const std::string path = resolveSoundPath(settings.sound_file);
        audio::DecodeResult result = path.empty()
            ? audio::DecodeResult{}
            : sound_decoder_.decodeFile(path, audio::kMaxSourceSoundSamples);
        if (result.status == audio::DecodeStatus::kOk) {
            result = audio::selectRange(result.sound, settings.start_ms, settings.end_ms);
        }
        if (result.status == audio::DecodeStatus::kOk &&
            !result.sound.empty()) {
            prepared_sound_ = std::move(result.sound);
        } else {
            client_.log("entrance sound could not be decoded; overlay disabled",
                        LogSeverity::kWarning);
        }
    }

    const audio::DecodedSound* prepared =
        prepared_sound_.empty() ? nullptr : &prepared_sound_;
    controller_.updateSettings(settings, prepared);
}

std::string PluginRuntime::resolveSoundPath(
    const std::string& configured) const {
    fs::path configured_path;
    try {
        configured_path = fs::u8path(configured);
    } catch (const std::exception&) {
        return {};
    }
    if (configured_path.is_absolute()) {
        return configured;
    }
    if (data_directory_.empty()) {
        return {};
    }
    return (fs::u8path(data_directory_) / configured_path).u8string();
}

}  // namespace ts3toolkit::plugin
