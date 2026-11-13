# create_plugin.cmake
#
# Builds a .ts3_plugin bundle (a ZIP archive with a custom extension) from a
# built plugin DLL and a generated package.ini. Designed to be executed in
# script mode (`cmake -P`) with the variables below supplied on the command
# line:
#
#   TS3TK_DLL              Absolute path to the built ts3toolkit.dll
#   TS3TK_PACKAGE_INI      Absolute path to the generated package.ini
#   TS3TK_STAGE_DIR        Working directory used to stage the archive layout
#   TS3TK_OUTPUT           Absolute path of the resulting .ts3_plugin
#   TS3TK_PLUGIN_DLL_NAME  File name of the DLL inside the archive. The TS3
#                          installer selects the platform-suffixed name
#                          (e.g. ts3toolkit_win64.dll) based on package.ini.

set(_required_variables
    TS3TK_DLL
    TS3TK_PACKAGE_INI
    TS3TK_STAGE_DIR
    TS3TK_OUTPUT
    TS3TK_PLUGIN_DLL_NAME
)
foreach(_variable IN LISTS _required_variables)
    if(NOT DEFINED ${_variable} OR "${${_variable}}" STREQUAL "")
        message(FATAL_ERROR
            "create_plugin.cmake: required variable ${_variable} is not set")
    endif()
endforeach()

if(NOT EXISTS "${TS3TK_DLL}")
    message(FATAL_ERROR
        "create_plugin.cmake: plugin DLL not found: ${TS3TK_DLL}")
endif()
if(NOT EXISTS "${TS3TK_PACKAGE_INI}")
    message(FATAL_ERROR
        "create_plugin.cmake: package.ini not found: ${TS3TK_PACKAGE_INI}")
endif()

# Recreate a clean staging area with the layout the TS3 package installer
# expects:
#   package.ini
#   plugins/<platform-suffixed plugin>.dll
file(REMOVE_RECURSE "${TS3TK_STAGE_DIR}")
file(MAKE_DIRECTORY "${TS3TK_STAGE_DIR}/plugins")

file(COPY "${TS3TK_PACKAGE_INI}" DESTINATION "${TS3TK_STAGE_DIR}")

file(COPY "${TS3TK_DLL}" DESTINATION "${TS3TK_STAGE_DIR}/plugins")
get_filename_component(_source_dll_name "${TS3TK_DLL}" NAME)
file(RENAME
    "${TS3TK_STAGE_DIR}/plugins/${_source_dll_name}"
    "${TS3TK_STAGE_DIR}/plugins/${TS3TK_PLUGIN_DLL_NAME}"
)

# Ensure the output directory exists, then archive the staged contents. The
# working directory is the staging root so archive entries use relative paths
# without any absolute or build-tree prefixes.
file(REMOVE "${TS3TK_OUTPUT}")
get_filename_component(_output_directory "${TS3TK_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cfv "${TS3TK_OUTPUT}" --format=zip .
    WORKING_DIRECTORY "${TS3TK_STAGE_DIR}"
    RESULT_VARIABLE _zip_result
    ERROR_VARIABLE _zip_error
)

if(NOT _zip_result EQUAL 0)
    message(FATAL_ERROR
        "create_plugin.cmake: failed to create archive: ${_zip_error}")
endif()

if(NOT EXISTS "${TS3TK_OUTPUT}")
    message(FATAL_ERROR
        "create_plugin.cmake: archive was not produced: ${TS3TK_OUTPUT}")
endif()

message(STATUS "Created plugin package: ${TS3TK_OUTPUT}")
