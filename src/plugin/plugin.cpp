#if defined(_WIN32)
#include <windows.h>
#endif

#include <memory>
#include <string>

#include "ts3_functions.h"
#include "ts3toolkit/plugin/plugin_runtime.h"
#include "ts3_client_api.h"

#if defined(_WIN32)
#define TS3_EXPORT extern "C" __declspec(dllexport)
#else
#define TS3_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {

constexpr int kPluginApiVersion = 26;

TS3Functions g_functions;
std::string g_plugin_id;
std::unique_ptr<ts3toolkit::plugin::Ts3ClientApi> g_client;
std::unique_ptr<ts3toolkit::plugin::PluginRuntime> g_runtime;

}  // namespace

TS3_EXPORT const char* ts3plugin_name() {
    return "TS3Toolkit";
}

TS3_EXPORT const char* ts3plugin_version() {
    return "0.1.0";
}

TS3_EXPORT int ts3plugin_apiVersion() {
    return kPluginApiVersion;
}

TS3_EXPORT const char* ts3plugin_author() {
    return "TS3Toolkit contributors";
}

TS3_EXPORT const char* ts3plugin_description() {
    return "Toolkit for TeamSpeak 3. First feature: personal entrance sound.";
}

TS3_EXPORT void ts3plugin_setFunctionPointers(
    const struct TS3Functions funcs) {
    g_functions = funcs;
}

TS3_EXPORT int ts3plugin_init() {
    // The plugin ID is supplied only after init via registerPluginID, so the
    // runtime is assembled there. No blocking work is required here.
    return 0;
}

TS3_EXPORT void ts3plugin_shutdown() {
    g_runtime.reset();
    g_client.reset();
    g_plugin_id.clear();
}

TS3_EXPORT void ts3plugin_registerPluginID(const char* id) {
    if (id != nullptr) {
        g_plugin_id.assign(id);
    }
    g_client = std::make_unique<ts3toolkit::plugin::Ts3ClientApi>(
        g_functions, g_plugin_id);
    g_runtime =
        std::make_unique<ts3toolkit::plugin::PluginRuntime>(*g_client);

    if (!g_runtime->initialize()) {
        // Fail closed: drop the half-initialized runtime but stay loaded so
        // the client does not treat the microphone path as broken.
        g_runtime.reset();
    }
}

TS3_EXPORT void ts3plugin_onClientMoveEvent(
    uint64 server_connection_handler_id, anyID client_id,
    uint64 old_channel_id, uint64 new_channel_id, int visibility,
    const char* move_message) {
    (void)old_channel_id;
    (void)visibility;
    (void)move_message;
    if (g_runtime) {
        g_runtime->onClientMove(server_connection_handler_id, client_id,
                                new_channel_id);
    }
}

TS3_EXPORT void ts3plugin_onClientMoveMovedEvent(
    uint64 server_connection_handler_id, anyID client_id,
    uint64 old_channel_id, uint64 new_channel_id, int visibility,
    anyID mover_id, const char* mover_name, const char* mover_unique_id,
    const char* move_message) {
    (void)old_channel_id;
    (void)visibility;
    (void)mover_id;
    (void)mover_name;
    (void)mover_unique_id;
    (void)move_message;
    if (g_runtime) {
        g_runtime->onClientMove(server_connection_handler_id, client_id,
                                new_channel_id);
    }
}

TS3_EXPORT void ts3plugin_onConnectStatusChangeEvent(
    uint64 server_connection_handler_id, int new_status,
    unsigned int error_number) {
    (void)error_number;
    if (g_runtime && new_status == STATUS_CONNECTION_ESTABLISHED) {
        g_runtime->onConnectionEstablished(server_connection_handler_id);
    }
}
