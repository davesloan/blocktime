# BlockTime
# Copyright (C) 2026 Dave Sloan
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

# DLL CRT (/MD) is required: the SDK's static SimConnect.lib is built with /MD (LNK2038 otherwise). D-019.
# SimConnect itself is still linked statically, so no SimConnect.dll ships; the VC++ runtime comes with MSFS.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

project(blocktime VERSION 0.10.0 LANGUAGES CXX)

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
  src/app_event_queue.cpp
  src/app_settings.cpp
  src/atc_log.cpp
  src/dates.cpp
  src/error_log.cpp
  src/event_log.cpp
  src/event_outbox.cpp
  src/block_detector.cpp
  src/catalog.cpp
  src/config_yaml.cpp
  src/csv.cpp
  src/flight_history.cpp
  src/flight_match.cpp
  src/match_reporter.cpp
  src/flight_settlement.cpp
  src/flight_stages.cpp
  src/flight_tracker.cpp
  src/geo.cpp
  src/gsx_timeline.cpp
  src/gsx_watcher.cpp
  src/leasing.cpp
  src/loads.cpp
  src/flight_draft.cpp
  src/planned_flight.cpp
  src/pricing.cpp
  src/rates.cpp
  src/runway_match.cpp
  src/parking.cpp
  src/lined_up.cpp
  src/gate_picker.cpp
  src/gate_assigner.cpp
  src/sayintentions.cpp
  src/simbrief.cpp
  src/money.cpp
  src/open_flight.cpp
  src/phase_detector.cpp
  src/sqlite_db.cpp
  src/telemetry_filter.cpp
  src/touchdown_capture.cpp
  src/cockpit_monitor.cpp
  src/flight_events.cpp
  src/debrief_measure.cpp
  src/debrief_score.cpp
  src/debrief_store.cpp
  src/debrief_review.cpp)
target_include_directories(blocktime-core PUBLIC src)
find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(blocktime-core PUBLIC blocktime-options nlohmann_json::nlohmann_json unofficial::sqlite3::sqlite3
  yaml-cpp::yaml-cpp)

# --- Sim: the recorder loop and SimConnect, run on its own thread by the app (D-053) ---
add_library(blocktime-sim STATIC
  src/recorder.cpp
  src/sim_connection.cpp
  src/sim_probe.cpp
  src/single_instance.cpp)
target_link_libraries(blocktime-sim PUBLIC blocktime-core SimConnect)

# PMDG's SDK broadcasts (D-159: read on every flight for the debrief; D-116: everything with --probe) need the SDK
# headers that ship with each PMDG aircraft. They aren't committed; point these at the folders holding them (in the
# PMDG package's Documentation\SDK). Without one, that half is a stub and its debrief checks show as not checked.
set(BLOCKTIME_PMDG_NG3_SDK_DIR "" CACHE PATH "Folder holding PMDG_NG3_SDK.h (737)")
set(BLOCKTIME_PMDG_777X_SDK_DIR "" CACHE PATH "Folder holding PMDG_777X_SDK.h (777)")
set(_pmdg_stub OFF)
if(BLOCKTIME_PMDG_NG3_SDK_DIR AND EXISTS "${BLOCKTIME_PMDG_NG3_SDK_DIR}/PMDG_NG3_SDK.h")
  target_sources(blocktime-sim PRIVATE src/pmdg_probe_ng3.cpp)
  set_source_files_properties(src/pmdg_probe_ng3.cpp PROPERTIES INCLUDE_DIRECTORIES "${BLOCKTIME_PMDG_NG3_SDK_DIR}")
else()
  target_compile_definitions(blocktime-sim PRIVATE BLOCKTIME_PMDG_NO_NG3)
  set(_pmdg_stub ON)
endif()
if(BLOCKTIME_PMDG_777X_SDK_DIR AND EXISTS "${BLOCKTIME_PMDG_777X_SDK_DIR}/PMDG_777X_SDK.h")
  target_sources(blocktime-sim PRIVATE src/pmdg_probe_777.cpp)
  set_source_files_properties(src/pmdg_probe_777.cpp PROPERTIES INCLUDE_DIRECTORIES "${BLOCKTIME_PMDG_777X_SDK_DIR}")
else()
  target_compile_definitions(blocktime-sim PRIVATE BLOCKTIME_PMDG_NO_777X)
  set(_pmdg_stub ON)
endif()
if(_pmdg_stub)
  target_sources(blocktime-sim PRIVATE src/pmdg_probe_none.cpp)
endif()

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
    tests/flight_match_test.cpp
    tests/flight_settlement_test.cpp
    tests/geo_test.cpp
    tests/leasing_test.cpp
    tests/loads_test.cpp
    tests/pricing_test.cpp
    tests/rates_test.cpp
    tests/simbrief_test.cpp
    tests/atis_test.cpp
    tests/planned_flight_test.cpp
    tests/flight_draft_test.cpp
    tests/money_test.cpp
    tests/open_flight_test.cpp
    tests/pmdg_probe_test.cpp
    tests/phase_detector_test.cpp
    tests/runway_match_test.cpp
    tests/parking_test.cpp
    tests/lined_up_test.cpp
    tests/gate_picker_test.cpp
    tests/gate_assigner_test.cpp
    tests/airspeed_display_test.cpp
    tests/touchdown_capture_test.cpp
    tests/flight_tracker_test.cpp
    tests/app_event_queue_test.cpp
    tests/atc_log_test.cpp
    tests/flight_stages_test.cpp
    tests/active_clock_test.cpp
    tests/gsx_watcher_test.cpp
    tests/gsx_timeline_test.cpp
    tests/cockpit_monitor_test.cpp
    tests/flight_events_test.cpp
    tests/debrief_measure_test.cpp
    tests/debrief_score_test.cpp
    tests/debrief_review_test.cpp)
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

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Location Positioning Network)
qt_standard_project_setup(REQUIRES 6.8)

# QML lives in qml/. Each file is aliased to the module root (qrc:/qt/qml/BlockTime/<name>.qml), so the files sit
# next to the module's qmldir and see each other and the Theme singleton without imports.
set(BLOCKTIME_QML_FILES
  qml/Main.qml
  qml/Theme.qml
  qml/Card.qml
  qml/StatTile.qml
  qml/StatusTile.qml
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
  qml/FlightCard.qml
  qml/FlightProgress.qml
  qml/SecondaryButton.qml
  qml/TextInputField.qml
  qml/MoneyLines.qml
  qml/NewFlightDialog.qml
  qml/PlannedFlightPanel.qml
  qml/NotRecordingBanner.qml
  qml/FlightSummary.qml
  qml/DebriefPanel.qml
  qml/CheckList.qml)
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
    src/ledger_model.h src/ledger_model.cpp
    src/planner.h
    src/qt_planner.h src/qt_planner.cpp
    src/new_flight_model.h src/new_flight_model.cpp
    src/planned_flight_model.h src/planned_flight_model.cpp
    src/flight_watcher.h src/flight_watcher.cpp
    src/review_client.h
    src/review_writer.h src/review_writer.cpp
    src/claude_reviewer.h src/claude_reviewer.cpp)
target_link_libraries(blocktime-app PRIVATE blocktime-sim Qt6::Quick Qt6::QuickControls2 Qt6::Location Qt6::Positioning
  Qt6::Network)
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

# The debrief replay tool (docs/debrief.md section 8): the checks, the score and Claude's request for a flight in a
# saved event log, with the app's own code. A console app; it runs with the Qt DLLs windeployqt put next to the app.
qt_add_executable(blocktime-debrief src/debrief_main.cpp src/review_client.h src/claude_reviewer.h
  src/claude_reviewer.cpp)
target_link_libraries(blocktime-debrief PRIVATE blocktime-core Qt6::Core Qt6::Network)
add_dependencies(blocktime-debrief blocktime-app)

# Tests for the Qt side (models and bridge, no QML). They run from the app's folder, so they use the Qt DLLs that
# windeployqt put there: the app is built first.
if(BLOCKTIME_BUILD_TESTS)
  qt_add_executable(blocktime-qt-tests
    tests/qt_models_test.cpp
    src/event_feed_model.h src/event_feed_model.cpp
    src/recorder_bridge.h src/recorder_bridge.cpp
    src/flight_list_model.h src/flight_list_model.cpp
    src/fleet_table_model.h src/fleet_table_model.cpp
    src/ledger_model.h src/ledger_model.cpp
    src/planner.h
    src/new_flight_model.h src/new_flight_model.cpp
    src/planned_flight_model.h src/planned_flight_model.cpp
    src/flight_watcher.h src/flight_watcher.cpp
    src/review_client.h
    src/review_writer.h src/review_writer.cpp)
  target_link_libraries(blocktime-qt-tests PRIVATE blocktime-sim Qt6::Gui Qt6::Qml Qt6::Positioning Qt6::Network
    GTest::gtest_main)
  target_include_directories(blocktime-qt-tests PRIVATE tests)
  target_compile_definitions(blocktime-qt-tests PRIVATE BLOCKTIME_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_dependencies(blocktime-qt-tests blocktime-app)
  gtest_discover_tests(blocktime-qt-tests)
endif()
