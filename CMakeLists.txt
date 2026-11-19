cmake_minimum_required(VERSION 3.16)

project(TS3Toolkit
    VERSION 0.1.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TS3TOOLKIT_BUILD_TESTS "Build TS3Toolkit unit tests" ON)

add_subdirectory(src)

if(TS3TOOLKIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Plugin package (.ts3_plugin)
#
# A .ts3_plugin is a ZIP archive with a custom extension that the TeamSpeak
# client opens through its bundled package installer. The ts3toolkit_package
# target stages the generated package.ini next to the platform-suffixed
# plugin DLL and zips the layout through packaging/create_plugin.cmake.
# ---------------------------------------------------------------------------
set(TS3TOOLKIT_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/package.ini.in"
    "${TS3TOOLKIT_PACKAGE_DIR}/package.ini"
    @ONLY
)

add_custom_target(ts3toolkit_package
    COMMAND "${CMAKE_COMMAND}"
        "-DTS3TK_DLL=$<TARGET_FILE:ts3toolkit>"
        "-DTS3TK_PACKAGE_INI=${TS3TOOLKIT_PACKAGE_DIR}/package.ini"
        "-DTS3TK_STAGE_DIR=${TS3TOOLKIT_PACKAGE_DIR}/stage"
        "-DTS3TK_OUTPUT=${TS3TOOLKIT_PACKAGE_DIR}/ts3toolkit-${PROJECT_VERSION}.ts3_plugin"
        "-DTS3TK_PLUGIN_DLL_NAME=ts3toolkit_win64.dll"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/packaging/create_plugin.cmake"
    DEPENDS
        ts3toolkit
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/create_plugin.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/package.ini.in"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building TS3Toolkit .ts3_plugin package"
    VERBATIM
)
