#include "ts3toolkit/plugin/talk_state_manager.h"

namespace ts3toolkit::plugin {

TalkStateManager::TalkStateManager(ClientApi& client)
    : client_(client),
      forcing_(false),
      saved_connection_id_(0),
      saved_{false, false} {}

bool TalkStateManager::beginForced(uint64_t connection_id) {
    if (forcing_) {
        return false;
    }

    // Snapshot the user's current configuration before changing anything.
    SavedState current{};
    if (!client_.getVoiceActivationEnabled(connection_id,
                                           current.voice_activation) ||
        !client_.getInputDeactivated(connection_id,
                                     current.input_deactivated)) {
        return false;
    }

    // Force continuous transmission: voice activation off and the local input
    // active. If any step fails, roll back to the snapshot so the user's
    // settings are never left half-applied.
    if (!client_.setVoiceActivationEnabled(connection_id, false) ||
        !client_.setInputDeactivated(connection_id, false) ||
        !client_.flushSelfUpdates(connection_id)) {
        client_.setVoiceActivationEnabled(connection_id,
                                          current.voice_activation);
        client_.setInputDeactivated(connection_id,
                                    current.input_deactivated);
        client_.flushSelfUpdates(connection_id);
        return false;
    }

    saved_ = current;
    saved_connection_id_ = connection_id;
    forcing_ = true;
    return true;
}

bool TalkStateManager::endForced(uint64_t /*connection_id*/) {
    if (!forcing_) {
        return false;
    }

    // Best-effort restore; even if a call fails, clear the forcing flag rather
    // than staying stuck and repeatedly attempting to restore.
    const bool vad_restored = client_.setVoiceActivationEnabled(
        saved_connection_id_, saved_.voice_activation);
    const bool input_restored = client_.setInputDeactivated(
        saved_connection_id_, saved_.input_deactivated);
    const bool flushed = client_.flushSelfUpdates(saved_connection_id_);

    forcing_ = false;
    saved_connection_id_ = 0;
    return vad_restored && input_restored && flushed;
}

void TalkStateManager::discardForced() {
    forcing_ = false;
    saved_connection_id_ = 0;
}

bool TalkStateManager::isForcing() const {
    return forcing_;
}

}  // namespace ts3toolkit::plugin
