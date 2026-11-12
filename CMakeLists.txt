cmake_minimum_required(VERSION 3.16)

project(TS3Toolkit
    VERSION 0.1.0
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
