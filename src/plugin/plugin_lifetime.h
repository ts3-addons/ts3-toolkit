#pragma once

#include <utility>

namespace ts3toolkit::plugin {

// Destruction must be explicit so DLL process detach never joins worker threads.
template <typename Client, typename Runtime>
struct PluginLifetime {
    PluginLifetime() = default;
    PluginLifetime(const PluginLifetime&) = delete;
    PluginLifetime& operator=(const PluginLifetime&) = delete;

    Client* client = nullptr;
    Runtime* runtime = nullptr;

    void resetRuntime() {
        delete std::exchange(runtime, nullptr);
    }

    void shutdown() {
        resetRuntime();
        delete std::exchange(client, nullptr);
    }
};

}  // namespace ts3toolkit::plugin
