set(TEST_TARGETS
    core_pcm_math_test
    core_mixer_test
    core_ring_buffer_test
    config_settings_test
    config_config_store_test
)

add_executable(core_pcm_math_test core/pcm_math_test.cpp)
add_executable(core_mixer_test core/mixer_test.cpp)
add_executable(core_ring_buffer_test core/ring_buffer_test.cpp)
add_executable(config_settings_test config/settings_test.cpp)
add_executable(config_config_store_test config/config_store_test.cpp)

foreach(test_target ${TEST_TARGETS})
    target_link_libraries(${test_target} PRIVATE ts3toolkit_core)
    target_include_directories(${test_target}
        PRIVATE ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()
