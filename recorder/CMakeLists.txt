# BlockTime Recorder
# Copyright (C) 2026 Dave Sloan
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

# DLL CRT (/MD) is required: the SDK's static SimConnect.lib is built with /MD (LNK2038 otherwise). D-019.
# SimConnect itself is still linked statically, so no SimConnect.dll ships; the VC++ runtime comes with MSFS.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

project(blocktime-recorder VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- SimConnect SDK -----------------------------------------------------------
# Not committed. Defaults to $env:MSFS_SDK\SimConnect SDK, else C:\MSFS SDK\SimConnect SDK.
if(DEFINED ENV{MSFS_SDK})
  file(TO_CMAKE_PATH "$ENV{MSFS_SDK}/SimConnect SDK" _simconnect_default)
else()
  set(_simconnect_default "C:/MSFS SDK/SimConnect SDK")
endif()
set(SIMCONNECT_SDK_DIR "${_simconnect_default}" CACHE PATH "Path to the MSFS 2020 'SimConnect SDK' folder")

if(NOT EXISTS "${SIMCONNECT_SDK_DIR}/include/SimConnect.h")
  message(FATAL_ERROR
    "SimConnect.h not found in '${SIMCONNECT_SDK_DIR}/include'. "
    "Install the MSFS 2020 SDK or pass -DSIMCONNECT_SDK_DIR=<path to 'SimConnect SDK'>.")
endif()

add_library(SimConnect STATIC IMPORTED)
set_target_properties(SimConnect PROPERTIES
  IMPORTED_LOCATION               "${SIMCONNECT_SDK_DIR}/lib/static/SimConnect.lib"
  IMPORTED_LOCATION_DEBUG         "${SIMCONNECT_SDK_DIR}/lib/static/SimConnect_debug.lib"
  IMPORTED_CONFIGURATIONS         "DEBUG;RELEASE"
  MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
  MAP_IMPORTED_CONFIG_MINSIZEREL     RELEASE
  INTERFACE_INCLUDE_DIRECTORIES   "${SIMCONNECT_SDK_DIR}/include"
  # Taken from the SDK's VS/SimConnectClient-static.props
  INTERFACE_LINK_LIBRARIES        "shlwapi;user32;Ws2_32")

# --- Third-party --------------------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)

# --- Shared build settings ----------------------------------------------------
# /external:W0 keeps third-party headers (gtest) from breaking the /W4 no-warnings rule.
add_library(blocktime-options INTERFACE)
target_compile_definitions(blocktime-options INTERFACE
  WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  BLOCKTIME_VERSION="${PROJECT_VERSION}")
target_compile_options(blocktime-options INTERFACE /W4 /permissive- /utf-8 /external:W0)

# --- Core: logic with no SimConnect or windows.h, so it can be unit tested (D-041) ---
add_library(blocktime-core STATIC
  src/event_log.cpp
  src/block_detector.cpp
  src/geo.cpp
  src/telemetry_filter.cpp)
target_include_directories(blocktime-core PUBLIC src)
target_link_libraries(blocktime-core PUBLIC blocktime-options nlohmann_json::nlohmann_json)

# --- Recorder -----------------------------------------------------------------
add_executable(blocktime-recorder
  src/main.cpp
  src/sim_connection.cpp)
target_link_libraries(blocktime-recorder PRIVATE blocktime-core SimConnect)

# --- Tests --------------------------------------------------------------------
option(BLOCKTIME_BUILD_TESTS "Build the unit tests" ON)
if(BLOCKTIME_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)
  add_executable(blocktime-tests
    tests/block_detector_test.cpp
    tests/event_log_test.cpp
    tests/geo_test.cpp)
  target_link_libraries(blocktime-tests PRIVATE blocktime-core GTest::gtest_main)
  gtest_discover_tests(blocktime-tests)
endif()
