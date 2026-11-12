#pragma once

#include "ts3toolkit/config/settings.h"

#include <string>

namespace ts3toolkit::config {

// Result category of a load attempt.
enum class LoadStatus {
    kLoaded,          // file parsed and validated successfully
    kCreatedDefaults, // file missing; default document returned
    kRejected         // unreadable or invalid; defaults returned, file preserved
};

struct LoadOutcome {
    SettingsDocument document;
    LoadStatus status;
};

// Loads and persists settings through one versioned key=value file.
//
// The parser is strict: malformed lines or out-of-range values reject the whole
// document instead of silently changing user data. Saves are atomic: the new
// content is written to a temporary file and renamed over the target.
class ConfigStore {
public:
    explicit ConfigStore(std::string file_path);

    LoadOutcome load() const;
    bool save(const SettingsDocument& document) const;

private:
    std::string file_path_;
};

}  // namespace ts3toolkit::config
