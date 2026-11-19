#if defined(_WIN32)
#include <windows.h>
#endif

#include <memory>
#include <string>
#include <type_traits>

#include "ts3_functions.h"
#include "plugin_definitions.h"
#include "ts3toolkit/plugin/plugin_runtime.h"
#include "ts3_client_api.h"
#include "settings_dialog.h"
#include "plugin_lifetime.h"

#if defined(_WIN32)
#define TS3_EXPORT extern "C" __declspec(dllexport)
#else
#define TS3_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {

constexpr int kPluginApiVersion = 26;

TS3Functions g_functions;
std::string g_plugin_id;
ts3toolkit::plugin::PluginLifetime<ts3toolkit::plugin::Ts3ClientApi,
                                  ts3toolkit::plugin::PluginRuntime> g_plugin;
static_assert(std::is_trivially_destructible_v<decltype(g_plugin)>);

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
    return "TheSkyC";
}

TS3_EXPORT const char* ts3plugin_description() {
    return "TeamSpeak 3 toolkit with customizable channel entrance sounds.";
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

TS3_EXPORT int ts3plugin_offersConfigure() {
    return PLUGIN_OFFERS_CONFIGURE_QT_THREAD;
}

TS3_EXPORT void ts3plugin_configure(void* handle, void* parent) {
    (void)handle;
    (void)parent;
    if (g_plugin.client) {
        ts3toolkit::plugin::showSettingsDialog(nullptr, g_plugin.client->configPath());
    }
}

TS3_EXPORT void ts3plugin_shutdown() {
    g_plugin.shutdown();
    g_plugin_id.clear();
}

TS3_EXPORT void ts3plugin_registerPluginID(const char* id) {
    g_plugin.shutdown();
    try {
        g_plugin_id = id ? id : "";
        auto client = std::make_unique<ts3toolkit::plugin::Ts3ClientApi>(g_functions, g_plugin_id);
        auto runtime = std::make_unique<ts3toolkit::plugin::PluginRuntime>(*client);
        g_plugin.client = client.release();
        g_plugin.runtime = runtime.release();

        if (!g_plugin.runtime->initialize()) {
            g_plugin.resetRuntime();
        }
    } catch (...) {
        // Never let a C++ exception escape into the TeamSpeak host.
        g_plugin.shutdown();
        if (g_functions.logMessage) {
            g_functions.logMessage("Plugin initialization failed with an exception",
                                   LogLevel_ERROR, "TS3Toolkit", 0);
        }
    }
}

TS3_EXPORT void ts3plugin_onClientMoveEvent(
    uint64 server_connection_handler_id, anyID client_id,
    uint64 old_channel_id, uint64 new_channel_id, int visibility,
    const char* move_message) {
    (void)old_channel_id;
    (void)visibility;
    (void)move_message;
    if (g_plugin.runtime) {
        g_plugin.runtime->onClientMove(server_connection_handler_id, client_id,
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
    if (g_plugin.runtime) {
        g_plugin.runtime->onClientMove(server_connection_handler_id, client_id,
                                new_channel_id);
    }
}

TS3_EXPORT void ts3plugin_onConnectStatusChangeEvent(
    uint64 server_connection_handler_id, int new_status,
    unsigned int error_number) {
    (void)error_number;
    if (g_plugin.runtime && new_status == STATUS_CONNECTION_ESTABLISHED) {
        g_plugin.runtime->onConnectionEstablished(server_connection_handler_id);
    } else if (g_plugin.runtime && new_status == STATUS_DISCONNECTED) {
        g_plugin.runtime->onConnectionDisconnected(server_connection_handler_id);
    }
}

TS3_EXPORT void ts3plugin_onEditCapturedVoiceDataEvent(
    uint64 server_connection_handler_id, short* samples, int sample_count,
    int channels, int* edited) {
    if (g_plugin.runtime && edited &&
        g_plugin.runtime->onCapturedAudio(server_connection_handler_id, samples,
                                          sample_count, channels)) {
        // Bit 0 marks modified samples; bit 1 requests transmission. Preserve
        // any flags the client or another plugin already supplied.
        *edited |= 3;
    }
}
