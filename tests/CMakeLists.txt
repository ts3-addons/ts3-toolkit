set(TEST_TARGETS
    core_pcm_math_test
    core_mixer_test
    core_ring_buffer_test
    config_settings_test
    config_config_store_test
    audio_sound_decoder_test
    audio_engine_test
    entrance_controller_test
)

add_executable(core_pcm_math_test core/pcm_math_test.cpp)
add_executable(core_mixer_test core/mixer_test.cpp)
add_executable(core_ring_buffer_test core/ring_buffer_test.cpp)
add_executable(config_settings_test config/settings_test.cpp)
add_executable(config_config_store_test config/config_store_test.cpp)
add_executable(audio_sound_decoder_test audio/sound_decoder_test.cpp)
add_executable(audio_engine_test audio/audio_engine_test.cpp)
add_executable(entrance_controller_test entrance/entrance_controller_test.cpp)

# Plugin-layer tests link the plugin static library (which publicly carries the
# core dependency) rather than core alone.
add_executable(plugin_talk_state_test plugin/talk_state_manager_test.cpp)

foreach(test_target ${TEST_TARGETS})
    target_link_libraries(${test_target} PRIVATE ts3toolkit_core)
    target_include_directories(${test_target}
        PRIVATE ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()

target_link_libraries(plugin_talk_state_test PRIVATE ts3toolkit_plugin)
target_include_directories(plugin_talk_state_test
    PRIVATE ${CMAKE_SOURCE_DIR}
)
add_test(NAME plugin_talk_state_test COMMAND plugin_talk_state_test)
