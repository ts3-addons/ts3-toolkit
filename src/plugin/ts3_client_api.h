#pragma once

#include "ts3toolkit/plugin/client_api.h"

#include "ts3_functions.h"

#include <string>

namespace ts3toolkit::plugin {

// Production ClientApi backed by the real TeamSpeak function table. The table
// is copied (it only holds function pointers) so this object owns no external
// lifetime. This header includes the SDK and stays out of the public include
// tree.
class Ts3ClientApi : public ClientApi {
public:
    Ts3ClientApi(TS3Functions functions, std::string plugin_id);

    bool registerCustomDevice(const std::string& device_id,
                              const std::string& display_name,
                              int capture_frequency,
                              int capture_channels) override;
    bool unregisterCustomDevice(const std::string& device_id) override;
    bool pushCaptureFrames(const std::string& device_id,
                           const core::Sample* frames,
                           int sample_count) override;

    bool getLocalClientId(uint64_t connection_id,
                          uint16_t& client_id) override;
    bool getChannelOfClient(uint64_t connection_id,
                            uint16_t client_id,
                            uint64_t& channel_id) override;
    uint64_t currentConnectionId() override;

    bool getVoiceActivationEnabled(uint64_t connection_id,
                                   bool& enabled) override;
    bool getInputDeactivated(uint64_t connection_id,
                             bool& deactivated) override;
    bool setVoiceActivationEnabled(uint64_t connection_id,
                                   bool enabled) override;
    bool setInputDeactivated(uint64_t connection_id,
                             bool deactivated) override;
    bool flushSelfUpdates(uint64_t connection_id) override;

    std::string configPath() override;
    std::string pluginPath() override;

    void log(const std::string& message, LogSeverity severity) override;

private:
    TS3Functions functions_;
    std::string plugin_id_;
};

}  // namespace ts3toolkit::plugin
