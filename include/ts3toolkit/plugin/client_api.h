#pragma once

#include "ts3toolkit/core/types.h"

#include <cstdint>
#include <string>

namespace ts3toolkit::plugin {

// Severity for client log messages; mapped to the SDK LogLevel at the edge.
enum class LogSeverity {
    kInfo,
    kWarning,
    kError,
};

// Abstract boundary to the TeamSpeak 3 client. The interface is SDK-free so
// runtime and device logic can be driven by a fake in unit tests. The only
// production implementation holds the TS3Functions table and lives in src.
class ClientApi {
public:
    virtual ~ClientApi() = default;

    // Register a custom capture device. The production side also supplies the
    // matching playback parameters required by the SDK even though only
    // capture is used. Returns false on a client error.
    virtual bool registerCustomDevice(const std::string& device_id,
                                      const std::string& display_name,
                                      int capture_frequency,
                                      int capture_channels) = 0;
    virtual bool unregisterCustomDevice(const std::string& device_id) = 0;

    // Push one buffer of captured, already-mixed PCM frames into the client.
    virtual bool pushCaptureFrames(const std::string& device_id,
                                   const core::Sample* frames,
                                   int sample_count) = 0;

    // Identity and channel queries.
    virtual bool getLocalClientId(uint64_t connection_id,
                                  uint16_t& client_id) = 0;
    virtual bool getChannelOfClient(uint64_t connection_id,
                                    uint16_t client_id,
                                    uint64_t& channel_id) = 0;
    virtual uint64_t currentConnectionId() = 0;

    // Filesystem roots reported by the client.
    virtual std::string configPath() = 0;
    virtual std::string pluginPath() = 0;

    // Bounded client log.
    virtual void log(const std::string& message, LogSeverity severity) = 0;
};

}  // namespace ts3toolkit::plugin
