add_library(ts3toolkit_core STATIC
    core/mixer.cpp
    core/ring_buffer.cpp
    config/settings.cpp
    config/config_store.cpp
    audio/miniaudio_impl.cpp
    audio/sound_decoder.cpp
    audio/microphone_capture.cpp
    audio/audio_engine.cpp
    platform/system_clock.cpp
    entrance/entrance_controller.cpp
)

target_include_directories(ts3toolkit_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/miniaudio
)

target_compile_features(ts3toolkit_core PUBLIC cxx_std_17)

# SDK-free plugin layer: device pump, playback adapter, and runtime wiring.
# It depends only on the abstract ClientApi boundary, never on the TS3 headers.
add_library(ts3toolkit_plugin STATIC
    plugin/audio_playback_adapter.cpp
    plugin/custom_capture_device.cpp
    plugin/plugin_runtime.cpp
    plugin/talk_state_manager.cpp
)

target_link_libraries(ts3toolkit_plugin PUBLIC ts3toolkit_core)

# Final shared plugin loaded by the TeamSpeak 3 client. The SDK-specific
# production ClientApi and the C export layer live here.
add_library(ts3toolkit SHARED
    plugin/plugin.cpp
    plugin/ts3_client_api.cpp
    plugin/settings_dialog.cpp
    plugin/settings_dialog.rc
)

target_include_directories(ts3toolkit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ts3-plugin-sdk
)

target_link_libraries(ts3toolkit PRIVATE ts3toolkit_plugin)
target_link_libraries(ts3toolkit PRIVATE comdlg32)
target_compile_options(ts3toolkit PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

set_target_properties(ts3toolkit PROPERTIES
    OUTPUT_NAME ts3toolkit
    PREFIX ""
)
