#include "ts3toolkit/plugin/plugin_runtime.h"

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
      custom_device_(client_, audio_engine_, kDeviceId, kDeviceDisplayName),
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
            (fs::path(plugins_root) / kPluginDataFolderName).string();
    }

    const fs::path config_file =
        fs::path(config_directory) / "ts3toolkit.ini";
    config_store_ =
        std::make_unique<config::ConfigStore>(config_file.string());

    config::LoadOutcome loaded = config_store_->load();
    settings_doc_ = std::move(loaded.document);
    if (loaded.status == config::LoadStatus::kRejected) {
        client_.log("settings file invalid; using defaults",
                    LogSeverity::kWarning);
    }

    // Decode before scheduling so the pump never performs file I/O.
    reloadSound(settings_doc_.entrance_sound);

    // Advance the controller on the pump's fixed cadence and keep the
    // transmission gating aligned with overlay playback.
    custom_device_.setFrameHook([this] {
        {
            std::unique_lock<std::mutex> lock(controller_mutex_, std::try_to_lock);
            if (lock.owns_lock()) {
                controller_.poll();
            }
        }
        syncTransmission();
    });

    if (!custom_device_.start()) {
        client_.log("failed to start custom capture device",
                    LogSeverity::kError);
        return false;
    }

    initialized_ = true;
    return true;
}

void PluginRuntime::shutdown() {
    if (!initialized_) {
        return;
    }

    custom_device_.stop();

    // Restore the user's original talk state after the pump has stopped.
    if (transmission_forced_) {
        talk_state_.endForced(client_.currentConnectionId());
        transmission_forced_ = false;
    }

    playback_adapter_.stopOverlay();
    config_store_.reset();
    prepared_sound_.samples.clear();
    data_directory_.clear();

    initialized_ = false;
}

void PluginRuntime::syncTransmission() {
    const bool playing = playback_adapter_.isOverlayPlaying();
    const uint64_t connection_id = client_.currentConnectionId();

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
        return;  // Disconnects and leaves never trigger.
    }

    // Enabled/configured/cooldown checks are owned by the controller.
    std::lock_guard<std::mutex> lock(controller_mutex_);
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

    std::lock_guard<std::mutex> lock(controller_mutex_);
    controller_.onLocalClientEnteredChannel();
}

void PluginRuntime::reloadSound(
    const config::EntranceSoundSettings& settings) {
    prepared_sound_.samples.clear();

    if (!settings.sound_file.empty()) {
        const std::string path = resolveSoundPath(settings.sound_file);
        audio::DecodeResult result = path.empty()
            ? audio::DecodeResult{}
            : sound_decoder_.decodeFile(path);
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
    const fs::path configured_path(configured);
    if (configured_path.is_absolute()) {
        return configured;
    }
    if (data_directory_.empty()) {
        return {};
    }
    return (fs::path(data_directory_) / configured_path).string();
}

}  // namespace ts3toolkit::plugin
