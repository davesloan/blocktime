# BlockTime
# Copyright (C) 2026 Dave Sloan
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

# DLL CRT (/MD) is required: the SDK's static SimConnect.lib is built with /MD (LNK2038 otherwise). D-019.
# SimConnect itself is still linked statically, so no SimConnect.dll ships; the VC++ runtime comes with MSFS.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

project(blocktime VERSION 0.5.0 LANGUAGES CXX)

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
find_package(unofficial-sqlite3 CONFIG REQUIRED)  # fleet store (D-061)

# --- Shared build settings ----------------------------------------------------
# /external:W0 keeps third-party headers (gtest) from breaking the /W4 no-warnings rule.
add_library(blocktime-options INTERFACE)
target_compile_definitions(blocktime-options INTERFACE
  WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  BLOCKTIME_VERSION="${PROJECT_VERSION}")
target_compile_options(blocktime-options INTERFACE /W4 /permissive- /utf-8 /external:W0)

# --- Core: logic with no SimConnect or windows.h, so it can be unit tested (D-041) ---
add_library(blocktime-core STATIC
  src/airline_config.cpp
  src/airline_db.cpp
  src/airline_queries.cpp
  src/airline_session.cpp
  src/airport_import.cpp
  src/app_settings.cpp
  src/dates.cpp
  src/error_log.cpp
  src/event_log.cpp
  src/event_outbox.cpp
  src/block_detector.cpp
  src/catalog.cpp
  src/config_yaml.cpp
  src/csv.cpp
  src/flight_history.cpp
  src/geo.cpp
  src/leasing.cpp
  src/loads.cpp
  src/flight_draft.cpp
  src/planned_flight.cpp
  src/pricing.cpp
  src/sayintentions.cpp
  src/simbrief.cpp
  src/money.cpp
  src/open_flight.cpp
  src/sqlite_db.cpp
  src/telemetry_filter.cpp)
target_include_directories(blocktime-core PUBLIC src)
find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(blocktime-core PUBLIC blocktime-options nlohmann_json::nlohmann_json unofficial::sqlite3::sqlite3
  yaml-cpp::yaml-cpp)

# --- Sim: the recorder loop and SimConnect, run on its own thread by the app (D-053) ---
add_library(blocktime-sim STATIC
  src/recorder.cpp
  src/sim_connection.cpp
  src/single_instance.cpp)
target_link_libraries(blocktime-sim PUBLIC blocktime-core SimConnect)

# --- Airport import: a console tool, so its counts show in the terminal (D-094) ---
add_executable(blocktime-import src/import_main.cpp)
target_link_libraries(blocktime-import PRIVATE blocktime-core)

# --- Tests --------------------------------------------------------------------
option(BLOCKTIME_BUILD_TESTS "Build the unit tests" ON)
if(BLOCKTIME_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)
  add_executable(blocktime-tests
    tests/airline_config_test.cpp
    tests/airline_db_test.cpp
    tests/airline_session_test.cpp
    tests/airport_import_test.cpp
    tests/app_settings_test.cpp
    tests/block_detector_test.cpp
    tests/catalog_test.cpp
    tests/csv_test.cpp
    tests/dates_test.cpp
    tests/error_log_test.cpp
    tests/event_log_test.cpp
    tests/event_outbox_test.cpp
    tests/flight_history_test.cpp
    tests/geo_test.cpp
    tests/leasing_test.cpp
    tests/loads_test.cpp
    tests/pricing_test.cpp
    tests/simbrief_test.cpp
    tests/atis_test.cpp
    tests/planned_flight_test.cpp
    tests/flight_draft_test.cpp
    tests/money_test.cpp
    tests/open_flight_test.cpp)
  target_link_libraries(blocktime-tests PRIVATE blocktime-core GTest::gtest_main)
  # Tests read the repo's config files (config/catalog.yaml, config/airline.example.yaml).
  target_compile_definitions(blocktime-tests PRIVATE BLOCKTIME_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  gtest_discover_tests(blocktime-tests)
endif()

# --- The app: Qt 6 + QML, one program (D-063) -----------------------------------
# Qt comes from the official Qt installer, not vcpkg. Defaults to $env:BLOCKTIME_QT_DIR, else the 6.12.0 kit.
# This section comes last: qt_standard_project_setup() turns on AUTOMOC for every target defined after it.
if(DEFINED ENV{BLOCKTIME_QT_DIR})
  file(TO_CMAKE_PATH "$ENV{BLOCKTIME_QT_DIR}" _qt_default)
else()
  set(_qt_default "C:/Qt/6.12.0/msvc2022_64")
endif()
set(BLOCKTIME_QT_DIR "${_qt_default}" CACHE PATH "Path to the Qt 6 MSVC 64-bit kit (the folder holding bin/ and lib/)")
list(APPEND CMAKE_PREFIX_PATH "${BLOCKTIME_QT_DIR}")

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Location Positioning)
qt_standard_project_setup(REQUIRES 6.8)

# QML lives in qml/. Each file is aliased to the module root (qrc:/qt/qml/BlockTime/<name>.qml), so the files sit
# next to the module's qmldir and see each other and the Theme singleton without imports.
set(BLOCKTIME_QML_FILES
  qml/Main.qml
  qml/Theme.qml
  qml/Card.qml
  qml/StatTile.qml
  qml/PrimaryButton.qml
  qml/StatusPill.qml
  qml/FlightPanel.qml
  qml/EventFeed.qml
  qml/Sidebar.qml
  qml/NavButton.qml
  qml/LivePage.qml
  qml/FlightsPage.qml
  qml/FleetPage.qml
  qml/MoneyPage.qml
  qml/RouteMap.qml
  qml/TrackDot.qml
  qml/FlightCard.qml)
foreach(_qml IN LISTS BLOCKTIME_QML_FILES)
  get_filename_component(_name "${_qml}" NAME)
  set_source_files_properties("${_qml}" PROPERTIES QT_RESOURCE_ALIAS "${_name}")
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(blocktime-app WIN32 src/app_main.cpp)
qt_add_qml_module(blocktime-app
  URI BlockTime
  VERSION 1.0
  QML_FILES ${BLOCKTIME_QML_FILES}
  SOURCES
    src/event_feed_model.h src/event_feed_model.cpp
    src/recorder_bridge.h src/recorder_bridge.cpp
    src/qml_hot_reload.h src/qml_hot_reload.cpp
    src/flight_list_model.h src/flight_list_model.cpp
    src/fleet_table_model.h src/fleet_table_model.cpp
    src/ledger_model.h src/ledger_model.cpp)
target_link_libraries(blocktime-app PRIVATE blocktime-sim Qt6::Quick Qt6::QuickControls2 Qt6::Location Qt6::Positioning)
set_target_properties(blocktime-app PROPERTIES OUTPUT_NAME BlockTime)  # BlockTime.exe (D-066)
# The C++ that qmlcachegen generates from our QML trips C4702 (unreachable code) inside Qt's qjsengine.h. It's raised
# by the optimizer, so /external:W0 can't hide it. Silence it for those generated files only.
foreach(_qml IN LISTS BLOCKTIME_QML_FILES)
  get_filename_component(_base "${_qml}" NAME_WE)
  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/blocktime-app_qml_${_base}_qml.cpp"
    TARGET_DIRECTORY blocktime-app PROPERTIES COMPILE_OPTIONS "/wd4702")
endforeach()
# Debug builds read QML from the source folder and reload it on save (D-055); release uses the compiled copy only.
target_compile_definitions(blocktime-app PRIVATE
  "$<$<CONFIG:Debug>:BLOCKTIME_QML_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/qml\">")

# Copy the Qt DLLs and QML plugins next to the exe, so it runs from the build folder without Qt on PATH (D-056).
# Position plugins are skipped: the app never reads a position source, and the NMEA one needs Qt SerialPort.
find_program(BLOCKTIME_WINDEPLOYQT windeployqt HINTS "${BLOCKTIME_QT_DIR}/bin" NO_DEFAULT_PATH REQUIRED)
add_custom_command(TARGET blocktime-app POST_BUILD
  COMMAND "${BLOCKTIME_WINDEPLOYQT}" --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" --no-translations
          --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime
          --skip-plugin-types position --verbose 0 "$<TARGET_FILE:blocktime-app>"
  COMMENT "windeployqt blocktime-app"
  VERBATIM)

# Tests for the Qt side (models and bridge, no QML). They run from the app's folder, so they use the Qt DLLs that
# windeployqt put there: the app is built first.
if(BLOCKTIME_BUILD_TESTS)
  qt_add_executable(blocktime-qt-tests
    tests/qt_models_test.cpp
    src/event_feed_model.h src/event_feed_model.cpp
    src/recorder_bridge.h src/recorder_bridge.cpp
    src/flight_list_model.h src/flight_list_model.cpp
    src/fleet_table_model.h src/fleet_table_model.cpp
    src/ledger_model.h src/ledger_model.cpp)
  target_link_libraries(blocktime-qt-tests PRIVATE blocktime-sim Qt6::Gui Qt6::Qml Qt6::Positioning GTest::gtest_main)
  add_dependencies(blocktime-qt-tests blocktime-app)
  gtest_discover_tests(blocktime-qt-tests)
endif()
