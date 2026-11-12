add_library(ts3toolkit_core STATIC
    core/mixer.cpp
    core/ring_buffer.cpp
    config/settings.cpp
    config/config_store.cpp
    audio/miniaudio_impl.cpp
    audio/sound_decoder.cpp
    audio/microphone_capture.cpp
    audio/audio_engine.cpp
)

target_include_directories(ts3toolkit_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/miniaudio
)

target_compile_features(ts3toolkit_core PUBLIC cxx_std_17)
