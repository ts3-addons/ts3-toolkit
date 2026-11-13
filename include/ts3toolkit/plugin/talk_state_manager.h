#pragma once

#include "ts3toolkit/plugin/client_api.h"

#include <cstdint>

namespace ts3toolkit::plugin {

// Temporarily switches a connection to continuous transmission so audio
// injected through a custom capture device is transmitted even when the user
// normally relies on push-to-talk or voice activation. The original talk
// configuration is captured before the change and restored afterwards.
//
// The class is SDK-free and drives the client only through the ClientApi
// boundary, so it can be exercised with a fake in unit tests.
class TalkStateManager {
public:
    explicit TalkStateManager(ClientApi& client);

    // Remember the current transmission settings and force continuous
    // transmission (voice activation off, input active). Returns false if the
    // manager is already forcing or the current state cannot be read or
    // changed; on a partial failure the client is left in its original state.
    bool beginForced(uint64_t connection_id);

    // Restore the settings saved by beginForced and stop forcing. Safe to call
    // when not forcing (returns false and changes nothing).
    bool endForced(uint64_t connection_id);

    bool isForcing() const;

private:
    // Snapshot of the user's original transmission configuration.
    struct SavedState {
        bool voice_activation;
        bool input_deactivated;
    };

    ClientApi& client_;
    bool forcing_;
    uint64_t saved_connection_id_;
    SavedState saved_;
};

}  // namespace ts3toolkit::plugin
