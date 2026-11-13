#pragma once

#include "ts3toolkit/audio/audio_engine.h"
#include "ts3toolkit/audio/decoded_sound.h"
#include "ts3toolkit/audio/sound_decoder.h"
#include "ts3toolkit/config/config_store.h"
#include "ts3toolkit/entrance/entrance_controller.h"
#include "ts3toolkit/plugin/audio_playback_adapter.h"
#include "ts3toolkit/plugin/client_api.h"
#include "ts3toolkit/plugin/custom_capture_device.h"
#include "ts3toolkit/plugin/talk_state_manager.h"
#include "ts3toolkit/platform/system_clock.h"

#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace ts3toolkit::plugin {

// Wires every subsystem together and owns its lifecycle. The C export layer
// forwards lifecycle and client events here. This class contains no TS3 SDK
// types and talks to the client only through the ClientApi boundary.
class PluginRuntime {
public:
    explicit PluginRuntime(ClientApi& client);
    ~PluginRuntime();

    PluginRuntime(const PluginRuntime&) = delete;
    PluginRuntime& operator=(const PluginRuntime&) = delete;

    bool initialize();
    void shutdown();

    // Client event entry points.
    void onClientMove(uint64_t connection_id, uint16_t client_id,
                      uint64_t new_channel_id);
    void onConnectionEstablished(uint64_t connection_id);
    void onConnectionDisconnected(uint64_t connection_id);

private:
    // (Re)decode the configured sound and refresh the controller wiring.
    void reloadSound(const config::EntranceSoundSettings& settings);
    std::string resolveSoundPath(const std::string& configured) const;

    // Keep the client's transmission gating aligned with overlay playback:
    // force continuous transmission while the sound is playing and restore the
    // user's prior talk state afterwards.
    void syncTransmission();

    static constexpr const char* kDeviceId = "ts3toolkit_capture";
    static constexpr const char* kDeviceDisplayName = "TS3Toolkit Virtual Capture";
    static constexpr const char* kPluginDataFolderName = "ts3toolkit";
    static constexpr std::size_t kCaptureRingCapacity =
        core::kFrameSamples * 10;  // ~100 ms microphone buffer

    ClientApi& client_;

    audio::AudioEngine audio_engine_;
    platform::SystemClock clock_;
    AudioPlaybackAdapter playback_adapter_;
    entrance::EntranceController controller_;
    std::mutex controller_mutex_;
    uint64_t controller_connection_id_ = 0;
    audio::SoundDecoder sound_decoder_;
    audio::DecodedSound prepared_sound_;
    CustomCaptureDevice custom_device_;
    TalkStateManager talk_state_;
    bool transmission_forced_;

    std::unique_ptr<config::ConfigStore> config_store_;
    config::SettingsDocument settings_doc_;
    std::string data_directory_;
    bool initialized_;
    std::atomic<bool> connection_active_{false};
};

}  // namespace ts3toolkit::plugin
