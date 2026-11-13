#include "ts3_client_api.h"

#include "teamlog/logtypes.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_errors.h"

#include <utility>

namespace ts3toolkit::plugin {

namespace {
constexpr int kPathBufferSize = 1024;
}

Ts3ClientApi::Ts3ClientApi(TS3Functions functions, std::string plugin_id)
    : functions_(functions), plugin_id_(std::move(plugin_id)) {}

bool Ts3ClientApi::registerCustomDevice(const std::string& device_id,
                                        const std::string& display_name,
                                        int capture_frequency,
                                        int capture_channels) {
    // The SDK requires playback parameters even for a capture-only device.
    // Mirror the capture format so the registered device stays internally
    // consistent without us implementing the playback side.
    const unsigned int error = functions_.registerCustomDevice(
        device_id.c_str(), display_name.c_str(), capture_frequency,
        capture_channels, capture_frequency, capture_channels);
    if (error != ERROR_ok) {
        log("custom capture registration failed, error=" +
                std::to_string(error), LogSeverity::kError);
    }
    return error == ERROR_ok;
}

bool Ts3ClientApi::unregisterCustomDevice(const std::string& device_id) {
    return functions_.unregisterCustomDevice(device_id.c_str()) == ERROR_ok;
}

bool Ts3ClientApi::pushCaptureFrames(const std::string& device_id,
                                     const core::Sample* frames,
                                     int sample_count) {
    return functions_.processCustomCaptureData(device_id.c_str(), frames,
                                               sample_count) == ERROR_ok;
}

bool Ts3ClientApi::getLocalClientId(uint64_t connection_id,
                                    uint16_t& client_id) {
    anyID local = 0;
    if (functions_.getClientID(connection_id, &local) != ERROR_ok) {
        return false;
    }
    client_id = static_cast<uint16_t>(local);
    return true;
}

bool Ts3ClientApi::getChannelOfClient(uint64_t connection_id,
                                      uint16_t client_id,
                                      uint64_t& channel_id) {
    uint64 channel = 0;
    if (functions_.getChannelOfClient(connection_id,
                                      static_cast<anyID>(client_id),
                                      &channel) != ERROR_ok) {
        return false;
    }
    channel_id = static_cast<uint64_t>(channel);
    return true;
}

uint64_t Ts3ClientApi::currentConnectionId() {
    return functions_.getCurrentServerConnectionHandlerID();
}

bool Ts3ClientApi::getVoiceActivationEnabled(uint64_t connection_id,
                                             bool& enabled) {
    char* raw = nullptr;
    if (functions_.getPreProcessorConfigValue(connection_id, "vad",
                                              &raw) != ERROR_ok) {
        return false;
    }
    enabled = (std::string(raw) == "true");
    functions_.freeMemory(raw);
    return true;
}

bool Ts3ClientApi::getInputDeactivated(uint64_t connection_id,
                                       bool& deactivated) {
    int value = 0;
    if (functions_.getClientSelfVariableAsInt(
            connection_id, CLIENT_INPUT_DEACTIVATED,
            &value) != ERROR_ok) {
        return false;
    }
    deactivated = (value == INPUT_DEACTIVATED);
    return true;
}

bool Ts3ClientApi::setVoiceActivationEnabled(uint64_t connection_id,
                                             bool enabled) {
    const char* value = enabled ? "true" : "false";
    return functions_.setPreProcessorConfigValue(
               connection_id, "vad", value) == ERROR_ok;
}

bool Ts3ClientApi::setInputDeactivated(uint64_t connection_id,
                                       bool deactivated) {
    const int value = deactivated ? INPUT_DEACTIVATED : INPUT_ACTIVE;
    return functions_.setClientSelfVariableAsInt(
               connection_id, CLIENT_INPUT_DEACTIVATED,
               value) == ERROR_ok;
}

bool Ts3ClientApi::flushSelfUpdates(uint64_t connection_id) {
    return functions_.flushClientSelfUpdates(
               connection_id, nullptr) == ERROR_ok;
}

std::string Ts3ClientApi::configPath() {
    char buffer[kPathBufferSize] = {0};
    functions_.getConfigPath(buffer, kPathBufferSize);
    return std::string(buffer);
}

std::string Ts3ClientApi::pluginPath() {
    char buffer[kPathBufferSize] = {0};
    functions_.getPluginPath(buffer, kPathBufferSize, plugin_id_.c_str());
    return std::string(buffer);
}

void Ts3ClientApi::log(const std::string& message, LogSeverity severity) {
    LogLevel level = LogLevel_INFO;
    switch (severity) {
        case LogSeverity::kInfo:
            level = LogLevel_INFO;
            break;
        case LogSeverity::kWarning:
            level = LogLevel_WARNING;
            break;
        case LogSeverity::kError:
            level = LogLevel_ERROR;
            break;
    }
    functions_.logMessage(message.c_str(), level, "TS3Toolkit", 0);
}

}  // namespace ts3toolkit::plugin
