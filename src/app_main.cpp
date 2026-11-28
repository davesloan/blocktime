// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The BlockTime app (D-063): a Qt Quick window around the recorder. The recorder runs on its own thread and writes the
// event log (D-053); QML only displays what the models expose (D-063).
//
// Options:
//   --data-dir <folder> keep all data there instead of %LOCALAPPDATA%\BlockTime (the debug build's default is
//                       %LOCALAPPDATA%\BlockTime-dev, a copy of the real folder made on its first run; D-226)
//   --log-dir <folder>  write the event log there instead of <data folder>\events
//   --demo              show made-up values and record nothing, to look at the UI without the sim; like --grab, it
//                       works on a scratch copy of the data that is deleted at exit (D-226)
//   --dev               developer mode (D-211): Live also shows the flight tiles and the event feed
//   --replay <flight>   play a settled flight (its log id, or "last") in a read-only window (D-175), with
//                       --speed <n>, --at <phase | block_out | block_in | H:MM> and --grab <png>

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSettings>
#include <QStringList>
#include <QStyleHints>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "airline_config.h"
#include "airline_session.h"
#include "app_settings.h"
#include "data_dir.h"
#include "claude_reviewer.h"
#include "review_writer.h"
#include "error_log.h"
#include "event_log.h"
#include "add_aircraft_model.h"
#include "aircraft_model.h"
#include "fleet_model.h"
#include "flight_list_model.h"
#include "airline_db.h"
#include "ledger_model.h"
#include "new_flight_model.h"
#include "planned_flight_model.h"
#include "dashboard_header.h"
#include "inflight_model.h"
#include "preflight_model.h"
#include "qml_hot_reload.h"
#include "window_placement.h"
#include "window_placement_win.h"
#include "flight_watcher.h"
#include "qt_planner.h"
#include "recorder.h"
#include "recorder_bridge.h"
#include "replay_session.h"
#include "single_instance.h"

namespace {

void copySimString(char* dst, std::size_t cap, std::string_view src) {
    std::memset(dst, 0, cap);
    std::memcpy(dst, src.data(), std::min(src.size(), cap - 1));
}

// --demo's made-up flight for the dashboard header: KBFI → KSAN along a rough route down the coast.
std::vector<blocktime::RoutePoint> demoRoute() {
    return {{47.53, -122.302}, {45.59, -122.6}, {42.37, -122.87}, {38.7, -121.6}, {35.4, -119.0}, {32.7336, -117.19}};
}

blocktime::PlannedFlightView demoFlightView() {
    blocktime::PlannedFlightView v;
    v.registration = "N737BT";
    v.typeCode = "B737-800";
    v.origin = "KBFI";
    v.originName = "Boeing Field King County International Airport";
    v.originCity = "Seattle";
    v.destination = "KSAN";
    v.destinationName = "San Diego International Airport";
    v.destinationCity = "San Diego";
    v.flightNumber = "1231";
    v.loadInParking = "Gate 4";
    v.originRunway = "14R";
    v.plannedAt = "2026-11-25T18:00:00Z";
    v.route = demoRoute();
    blocktime::SimbriefPlan p;
    p.requestId = "demo";
    p.cruiseAltitudeFt = 35000;
    p.estBlockS = 8700;
    p.originTransitionAltitudeFt = 18000;
    p.destinationTransitionLevelFt = 18000;
    p.originRunway = "14R";
    v.plan = p;
    return v;
}

// Made-up cruise values for --demo. Nothing is written to any log.
void fillDemo(RecorderBridge& bridge) {
    using namespace std::chrono_literals;
    blocktime::TelemetrySample s{};
    s.parkingBrake = 0;
    s.onGround = 0;
    s.fuelTotalLb = 18452;
    s.indicatedAltitudeFt = 35003;
    s.verticalSpeedFpm = -12;
    s.groundSpeedKt = 463;
    s.indicatedAirspeedKt = 268;
    s.mach = 0.784;
    s.numEngines = 2;
    s.engCombustion[0] = s.engCombustion[1] = 1;
    copySimString(s.title, sizeof(s.title), "PMDG 737-800 (demo)");
    copySimString(s.atcId, sizeof(s.atcId), "N737BT");

    const auto now = std::chrono::system_clock::now();
    blocktime::BlockOut out;
    out.time = now - 1h - 23min - 41s;
    out.fuelLb = 26310;

    bridge.applyConnection(blocktime::ConnectionState::Connected);
    bridge.applyLine("Demo mode: made-up values, nothing is recorded.", false);
    bridge.applyLine("Connected to sim.", false);
    const std::string released = blocktime::formatUtc(out.time).substr(11, 8);
    bridge.applyLine(QString::fromStdString(released + "  BLOCK OUT  N737BT  released " + released + "  fuel 26310 lb"),
                     false);
    bridge.applySample(s);
    // GSX's refuelling and boarding before the push, and a GSX push (D-138).
    const auto gsx = [&](const char* service, const char* state, std::chrono::seconds before) {
        bridge.applyFlightEvent("gsx_service", {{"service", service}, {"state", state},
                                                {"time", QString::fromStdString(blocktime::formatUtc(out.time - before))}});
    };
    gsx("refuelling", "performing", 31min);
    gsx("refuelling", "completed", 24min);
    gsx("boarding", "performing", 22min);
    gsx("boarding", "available", 4min);
    gsx("pushback", "performing", 10s);
    bridge.applyBlockEvent(out);

    // A made-up departure and climb to FL350, so the Flight progress card has something to show (D-128).
    const auto at = [&](std::chrono::seconds after) { return QString::fromStdString(blocktime::formatUtc(out.time + after)); };
    const auto phase = [&](const char* p, std::chrono::seconds after, int alt, bool fl = false) {
        bridge.applyFlightEvent("phase_changed", {{"phase", p}, {"time", at(after)}, {"alt_ft", alt}, {"flight_level", fl}});
    };
    phase("pushback", 0s, 20);
    phase("taxi_out", 3min, 20);
    phase("takeoff", 9min, 20);
    bridge.applyFlightEvent("liftoff", {{"time", at(9min + 35s)}, {"runway", "14R"}, {"airport", "KBFI"}});
    phase("climb", 10min, 1200);
    bridge.applyFlightEvent("level_off", {{"phase", "climb"}, {"time", at(11min)}, {"alt_ft", 5000}, {"flight_level", false}});
    bridge.applyFlightEvent("level_off_ended", {{"phase", "climb"}, {"time", at(12min)}, {"alt_ft", 5000}, {"flight_level", false}});
    bridge.applyFlightEvent("flight_marker", {{"marker", "above_10000"}, {"time", at(14min)}, {"alt_ft", 10000}});
    bridge.applyFlightEvent("flight_marker", {{"marker", "transition_altitude"}, {"time", at(17min)}, {"alt_ft", 18000}});
    // Samples down the demo route to a little past halfway, climbing to FL350, for the header's arc (D-182).
    const std::vector<blocktime::RoutePoint> route = demoRoute();
    constexpr int kSteps = 55;
    for (int i = 1; i <= kSteps; ++i) {
        const double along = i / 100.0 * static_cast<double>(route.size() - 1);  // 0.55 of the way at the end
        const auto leg = static_cast<std::size_t>(along);
        const double f = along - static_cast<double>(leg);
        blocktime::TelemetrySample p = s;
        p.latitudeDeg = route[leg].lat + (route[leg + 1].lat - route[leg].lat) * f;
        p.longitudeDeg = route[leg].lon + (route[leg + 1].lon - route[leg].lon) * f;
        p.indicatedAltitudeFt = std::min(35003.0, 35003.0 * i / 18.0);
        bridge.applySample(p);
    }
    phase("cruise", 31min, 35000, true);
}

struct MapSettings {
    std::string key;  // the CARTO key: never print or log it
    QString tileUrl;  // holds the key too
    QString cacheDir;
    QString userAgent;
};

// Qt messages with every key taken out (review finding #14): a network error can quote a tile or getWX URL, and
// QT_FORCE_STDERR_LOGGING (docs/notes/qt-setup.md) sends messages to stderr, which is the error log (D-067).
std::vector<std::string> g_secrets;  // the CARTO and SayIntentions keys
QtMessageHandler g_previousHandler = nullptr;

void redactingMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    const QString safe =
        g_secrets.empty() ? message : QString::fromStdString(blocktime::redact(message.toStdString(), g_secrets));
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, safe);
    } else {
        std::fputs(qPrintable(qFormatLogMessage(type, context, safe) + u'\n'), stderr);
    }
}

// Map tiles (D-059): CARTO Dark Matter through the osm plugin's custom URL, which fills in %z/%x/%y even with the
// key after them (probed 2026-11-22, docs/notes/qt-setup.md). Each URL gets its own cache folder: CARTO answers a
// wrong key with a normal 200 "API KEY REQUIRED" tile, and the cache's file names don't include the URL, so those
// tiles would otherwise outlive a fixed key.
MapSettings mapSettings() {
    // CARTO's terms allow caching tiles for 30 days at most; this also clears folders of old keys.
    blocktime::pruneOldFiles(blocktime::defaultTileCacheDir(), blocktime::kTileMaxAge);

    MapSettings m;
    m.userAgent = QStringLiteral("BlockTime/" BLOCKTIME_VERSION " (open source)");
    const std::string key = blocktime::loadAppSettings(blocktime::defaultSettingsFile()).cartoKey;
    if (key.empty()) return m;
    m.key = key;
    m.tileUrl = QStringLiteral("https://basemaps.cartocdn.com/rastertiles/dark_all/%z/%x/%y.png?key=") +
                QString::fromLatin1(QUrl::toPercentEncoding(QString::fromStdString(key)));
    const QString folder =
        QString::fromLatin1(QCryptographicHash::hash(m.tileUrl.toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
    m.cacheDir = QString::fromStdWString((blocktime::defaultTileCacheDir() / folder.toStdWString()).wstring());
    return m;
}

// Where this run keeps its data (D-226). Development never writes to Dave's airline: the debug build has its own
// folder, a copy of the real one made on its first run, and --demo and --grab (outside --replay, which is read-only)
// work on a scratch copy that is deleted at exit.
struct DataFolders {
    std::filesystem::path source;      // the data folder, or what a scratch run copied
    std::filesystem::path scratchDir;  // a scratch run's copy; "" otherwise
    bool scratch = false;
    std::vector<std::string> notes;  // for errors.log, once it's open
};

DataFolders chooseDataFolders(const QStringList& args) {
    DataFolders f;
    bool scratchRun = false;
    bool replay = false;
    std::optional<std::filesystem::path> chosen;
    for (qsizetype i = 1; i < args.size(); ++i) {
        if (args[i] == "--data-dir" && i + 1 < args.size()) chosen = args[++i].toStdWString();
        else if (args[i] == "--demo" || args[i] == "--grab") scratchRun = true;
        else if (args[i] == "--replay") replay = true;
    }
    std::string error;
    if (chosen) {
        f.source = *chosen;
    } else {
#ifdef NDEBUG
        f.source = blocktime::realDataDir();
#else
        f.source = blocktime::realDataDir().parent_path() / "BlockTime-dev";
        std::error_code ec;
        if (!std::filesystem::exists(f.source / "blocktime.db", ec)) {
            f.notes.push_back(blocktime::seedDataDir(blocktime::realDataDir(), f.source, true, &error)
                                  ? "Debug build: made " + f.source.string() + " from a copy of " +
                                        blocktime::realDataDir().string() + " (D-226)."
                                  : "Debug build: couldn't copy the real data folder: " + error);
        }
#endif
    }
    if (scratchRun && !replay) {
        f.scratch = true;
        std::error_code ec;
        f.scratchDir = std::filesystem::temp_directory_path(ec) /
                       ("BlockTime-scratch-" + std::to_string(QCoreApplication::applicationPid()));
        // Copies left by a run that was killed (they hold settings.json's keys): anything a day old goes.
        const auto dayAgo = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24);
        for (std::filesystem::directory_iterator it(f.scratchDir.parent_path(), ec), end; !ec && it != end;
             it.increment(ec)) {
            std::error_code itemEc;
            if (it->path().filename().wstring().starts_with(L"BlockTime-scratch-") &&
                it->last_write_time(itemEc) < dayAgo && !itemEc) {
                std::filesystem::remove_all(it->path(), itemEc);
            }
        }
        std::filesystem::remove_all(f.scratchDir, ec);
        if (!blocktime::seedDataDir(f.source, f.scratchDir, false, &error)) {
            f.notes.push_back("Couldn't make the scratch copy of the data: " + error);
        }
        blocktime::setDataDir(f.scratchDir);
    } else {
        blocktime::setDataDir(f.source);
    }
    return f;
}

// Deletes a scratch run's copy at exit. Declared before anything that opens it, so it goes last.
struct RemoveScratch {
    std::filesystem::path dir;
    ~RemoveScratch() {
        std::error_code ec;
        if (!dir.empty()) std::filesystem::remove_all(dir, ec);
    }
};

QStringList toQStringList(const std::vector<std::string>& lines) {
    QStringList out;
    for (const std::string& line : lines) out << QString::fromStdString(line);
    return out;
}

// --grab: saves the window after 6 s and quits. With QT_QPA_PLATFORM=offscreen nothing shows on screen
// (docs/notes/qt-setup.md).
void grabAfterStartup(QQmlApplicationEngine& engine, const QString& grabFile) {
    QTimer::singleShot(6000, QCoreApplication::instance(), [&engine, grabFile] {
        const QList<QObject*> roots = engine.rootObjects();
        if (auto* window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first())) {
            window->grabWindow().save(grabFile);
        }
        QCoreApplication::quit();
    });
}

struct ReplayOptions {
    QString flight;  // a log flight id, or "last"
    double speed = 1;
    QString at;
    QString grabFile;
    bool dev = false;  // --dev (D-211)
};

// The replay window (D-175): a separate, read-only process. No recorder thread or mutex, no AirlineSession (no
// migrations, lease settling or config import), the database opened read-only, and nothing that writes or calls out.
int runReplay(const ReplayOptions& options, const std::filesystem::path& logDir, const MapSettings& map) {
    const blocktime::AirlinePaths paths = blocktime::defaultAirlinePaths();
    std::string error;
    std::optional<blocktime::db::Database> opened = blocktime::db::Database::openReadOnly(paths.database, &error);
    if (!opened) std::cerr << "Replay: couldn't open the airline database: " << error << std::endl;
    blocktime::db::Database db = opened ? std::move(*opened) : blocktime::db::Database::openInMemory();
    if (!opened) blocktime::migrateAirlineDatabase(db, nullptr);

    QString flightId = options.flight;
    if (flightId == "last") flightId = QString::fromStdString(blocktime::latestFlightId(db).value_or(""));
    const auto config = blocktime::loadAirlineConfig(paths.airlineConfig, nullptr);
    const QString airlineIcao = config ? QString::fromStdString(config->identity.icaoCode) : QString();
    const QString airlineName = config ? QString::fromStdString(config->identity.name) : QString();
    const blocktime::Rates rates = blocktime::loadRates(blocktime::defaultRatesFile(), nullptr).value_or(blocktime::Rates{});

    FlightListModel flights(db, logDir, airlineIcao);
    flights.reload();
    ReplaySession replay(logDir, flightId, blocktime::flownFlightView(db, flightId.toStdString()), airlineIcao, rates,
                         airlineName);
    replay.setSpeed(options.speed);
    if (!options.at.isEmpty() && !replay.seek(options.at)) {
        std::cerr << "Replay: the flight has no " << options.at.toStdString() << std::endl;
    }
    // A screenshot holds where --at put it (or at the start).
    if (!options.grabFile.isEmpty()) replay.setPaused(true);
    replay.start();

    QQmlApplicationEngine engine;
    const QVariantMap props{{"replay", QVariant::fromValue(&replay)},
                            {"flights", QVariant::fromValue(&flights)},
                            {"devMode", options.dev},
                            {"mapTileUrl", map.tileUrl},
                            {"mapCacheDir", map.cacheDir},
                            {"mapUserAgent", map.userAgent}};
#ifdef BLOCKTIME_QML_SOURCE_DIR
    QGuiApplication::setQuitOnLastWindowClosed(false);
    // Closing the replay window ends the replay; hot reload otherwise keeps the app alive between reloads.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &engine, [](QObject* root) {
        if (auto* window = qobject_cast<QQuickWindow*>(root)) {
            QObject::connect(window, &QQuickWindow::closing, QCoreApplication::instance(), &QCoreApplication::quit);
        }
    });
    QmlHotReload hotReload(engine, QStringLiteral(BLOCKTIME_QML_SOURCE_DIR), props, QStringLiteral("ReplayWindow"));
    hotReload.load();
#else
    engine.setInitialProperties(props);
    engine.loadFromModule("BlockTime", "ReplayWindow");
    if (engine.rootObjects().isEmpty()) return 1;
#endif
    if (!options.grabFile.isEmpty()) grabAfterStartup(engine, options.grabFile);
    return QGuiApplication::exec();
}

}  // namespace

int main(int argc, char* argv[]) {
    // Basic is the Qt Quick Controls style meant to be fully re-skinned (D-054).
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("BlockTime");
    QGuiApplication::setApplicationVersion(BLOCKTIME_VERSION);
    const QStringList args = QGuiApplication::arguments();
    const DataFolders folders = chooseDataFolders(args);
    const RemoveScratch removeScratch{folders.scratchDir};
    // QML's own preferences (the dashboard header collapsed, D-187) in %LOCALAPPDATA%\BlockTime\BlockTime.ini, next to
    // the rest of BlockTime's data rather than in the registry (D-188).
    // QSettings puts it at <path>\<organization>\BlockTime.ini, so the organization is the data folder's own name:
    // %LOCALAPPDATA%\BlockTime\BlockTime.ini as before, and inside BlockTime-dev or a scratch copy for those (D-226).
    QGuiApplication::setOrganizationName(QString::fromStdWString(blocktime::dataDir().filename().wstring()));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QString::fromStdWString(blocktime::dataDir().parent_path().wstring()));
    // Dark only: this also gives the window a dark Windows title bar (D-054).
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    // A windowed app has no console, so std::cerr would go nowhere: LOST: events (D-049) and the errors EventLog and
    // SimConnection print go to errors.log next to the event log instead, or to %TEMP% if that can't be opened
    // (D-067). Declared before the recorder thread, so it outlives it.
    // A scratch run logs to the folder it copied, since its own is deleted at exit.
    const std::filesystem::path errorDir = folders.source;
    std::error_code dirError;
    std::filesystem::create_directories(errorDir, dirError);
    const blocktime::ErrorLog errorLog(
        {errorDir / "errors.log", std::filesystem::temp_directory_path(dirError) / "BlockTime-errors.log"}, std::cerr,
        "--- BlockTime " BLOCKTIME_VERSION " started " + blocktime::utcTimestamp() + " with data in " +
            blocktime::dataDir().string());
    for (const std::string& line : folders.notes) std::cerr << line << std::endl;

    const MapSettings map = mapSettings();
    const blocktime::AppSettings settings = blocktime::loadAppSettings(blocktime::defaultSettingsFile());
    g_secrets = settings.secrets();
    g_previousHandler = qInstallMessageHandler(redactingMessageHandler);

    // A scratch run reads the copied folder's event logs in place; it records nothing, so it never writes to them.
    std::filesystem::path logDir = folders.scratch ? folders.source / "events" : blocktime::EventLog::defaultDir();
    bool demo = false;
    bool dev = false;   // --dev: Live's flight tiles and event feed (D-211)
    QString grabFile;   // --grab <png>: a developer check of the screens; saves the window after 6 s and quits
    int startPage = 0;  // --page <n>: the page shown first (0 Live, 1 Flights, 2 Fleet, 3 Money)
    blocktime::RecorderOptions recorderOptions;  // --probe (D-116)
    ReplayOptions replay;                        // --replay (D-175)
    for (qsizetype i = 1; i < args.size(); ++i) {
        if (args[i] == "--data-dir" && i + 1 < args.size()) {
            ++i;  // chooseDataFolders read it
        } else if (args[i] == "--log-dir" && i + 1 < args.size()) {
            logDir = args[++i].toStdWString();
        } else if (args[i] == "--demo") {
            demo = true;
        } else if (args[i] == "--dev") {
            dev = true;
        } else if (args[i] == "--grab" && i + 1 < args.size()) {
            grabFile = args[++i];
        } else if (args[i] == "--page" && i + 1 < args.size()) {
            startPage = args[++i].toInt();
        } else if (args[i] == "--probe") {
            recorderOptions.probe = true;
        } else if (args[i] == "--replay" && i + 1 < args.size()) {
            replay.flight = args[++i];
        } else if (args[i] == "--speed" && i + 1 < args.size()) {
            replay.speed = args[++i].toDouble();
        } else if (args[i] == "--at" && i + 1 < args.size()) {
            replay.at = args[++i];
        }
    }

    if (!replay.flight.isEmpty()) {
        replay.grabFile = grabFile;
        replay.dev = dev;
        if (replay.speed <= 0) replay.speed = 1;
        return runReplay(replay, logDir, map);
    }

    // Declared before the thread, so they outlive it.
    RecorderBridge bridge;
    BridgeObserver observer(bridge);
    blocktime::FlightTargetSlot flightTarget;  // the planned flight, for the recorder's matching check (D-088)
    blocktime::AppEventQueue appEvents;        // the app's web results, written to the log by the recorder (D-127)
    // A scratch run (--demo, --grab) records nothing, so it doesn't take the recorder mutex (a recording copy can run
    // beside it).
    std::optional<blocktime::SingleInstance> instance;
    if (!folders.scratch) instance.emplace();
    std::atomic<bool> stop{false};
    std::jthread worker;
    // If aboutToQuit is ever skipped, this still stops and joins the recorder before what it uses goes (review #8).
    struct StopAndJoin {
        std::atomic<bool>& stop;
        std::jthread& worker;
        ~StopAndJoin() {
            stop = true;
            if (worker.joinable()) worker.join();
        }
    } stopAndJoin{stop, worker};

    // --demo's header shows a made-up flight of its own (the demo has no planned flight), built before the feed.
    std::unique_ptr<ReplayPlannedFlight> demoPlanned;
    std::unique_ptr<DashboardHeader> demoHeader;
    if (demo) {
        const auto demoConfig = blocktime::loadAirlineConfig(blocktime::defaultAirlinePaths().airlineConfig, nullptr);
        const QString icao = demoConfig ? QString::fromStdString(demoConfig->identity.icaoCode) : QStringLiteral("XLY");
        const QString name = demoConfig ? QString::fromStdString(demoConfig->identity.name) : QStringLiteral("Demo");
        demoPlanned = std::make_unique<ReplayPlannedFlight>(demoFlightView(), icao, blocktime::Rates{}, bridge, "demo");
        demoHeader =
            std::make_unique<DashboardHeader>(bridge, *demoPlanned, icao, name, DashboardHeader::SettledFlight{});
        fillDemo(bridge);
    } else if (folders.scratch) {
        // --grab without --demo: the real screens on the copied data, with no recorder.
    } else if (instance->acquired()) {
        worker = std::jthread([&] {
            int result = 2;
            try {
                result = blocktime::Recorder(logDir, observer, flightTarget, appEvents, recorderOptions).run(stop);
            } catch (const std::exception& e) {
                // Uncaught, it would end the app (review 3.4); the open flight is closed as recorder_crash next start.
                std::cerr << "The recorder stopped: " << e.what() << std::endl;
            }
            QMetaObject::invokeMethod(&bridge, [&bridge, result] { bridge.applyStopped(result); },
                                      Qt::QueuedConnection);
        });
    } else {
        bridge.setBlocked();
    }

    // Closing the window ends the recording like Ctrl+C does: an open flight is logged as interrupted
    // (recorder_exit) and recorder_stopped is written before the process exits.
    const auto stopRecorder = [&] {
        stop = true;
        if (worker.joinable()) worker.join();
    };
    QObject::connect(&app, &QCoreApplication::aboutToQuit, stopRecorder);

    // Flights come from the log (D-060), read on a worker thread. A block out or block in changes the list: the
    // recorder writes the line before it tells the bridge, so the reload sees it. (A block event whose write was held,
    // D-049, appears at the next reload after it's written.)

    // The airline: the database, the config files and the leases (docs/database.md). --demo opens its scratch copy
    // (D-226). The leases are settled once a flown flight has been
    // (below), and again once a minute, so a month that falls due while the app is open is charged (D-096).
    blocktime::AirlineSession airline = blocktime::AirlineSession::open(blocktime::defaultAirlinePaths());
    LedgerModel ledger(airline.ledger(), airline.cash());

    // Planning (design.md §13.1-13.3): New Flight and the planned flight before departure. Without a database the
    // models work on an empty one, so the screens still open.
    blocktime::db::Database emptyDatabase = blocktime::db::Database::openInMemory();
    if (!airline.database()) blocktime::migrateAirlineDatabase(emptyDatabase, nullptr);
    blocktime::db::Database& planningDb = airline.database() ? *airline.database() : emptyDatabase;
    QtPlanner planner(settings);
    // SayIntentions' ATIS and ATC log while a flight is open, into the event log (D-123, D-127).
    FlightWatcher flightWatcher(planner, bridge, flightTarget, appEvents);
    // The arrival gate (D-141, D-147): from the planned flight's type and kind, read when SayIntentions has picked.
    flightWatcher.setGateContext([&airline, &planningDb](const QString& destination)
                                     -> std::optional<blocktime::GateContext> {
        const auto planned = blocktime::currentPlannedFlight(planningDb);
        if (!planned) return std::nullopt;
        return airline.gateContext(planned->typeCode, planned->kind, destination.toStdString());
    });
    const QString airlineIcao = QString::fromStdString(airline.airlineIcao());
    // The Fleet screen (D-241): the airline, its slots and a card per airplane, marking the planned flight's plane.
    const auto fleetState = [&airline, &planningDb, airlineIcao] {
        FleetState s;
        s.airplanes = airline.airplanes();
        s.notices = toQStringList(airline.notices());
        s.hoursFlown = airline.hoursFlown();
        s.cashCents = airline.cash();
        s.airlineName = QString::fromStdString(airline.airlineName());
        s.airlineCode = airlineIcao;
        s.callsign = QString::fromStdString(airline.airlineCallsign());
        if (const auto planned = blocktime::currentPlannedFlight(planningDb)) {
            s.plannedRegistration = QString::fromStdString(planned->registration);
            s.plannedLeg = QStringLiteral("%1 %2 \u00b7 %3 \u2192 %4")
                               .arg(airlineIcao, QString::fromStdString(planned->flightNumber),
                                    QString::fromStdString(planned->origin), QString::fromStdString(planned->destination));
        }
        return s;
    };
    FleetModel fleet(fleetState());
    // Settled flights from the database, tracks from the log (D-111); refreshed when a flight is settled.
    // Claude's review of each settled flight (D-161): one automatic try after settling, then Retry on the summary.
    // A scratch run (D-226) could settle a pending flight in its copy; it mustn't spend a Claude review on it.
    ClaudeReviewer claude(folders.scratch ? std::string() : settings.claudeApiKey);
    ReviewWriter reviews(planningDb, logDir, claude);
    FlightListModel flights(planningDb, logDir, airlineIcao, &reviews);
    flights.reload();
    NewFlightModel newFlight(planningDb, airlineIcao, blocktime::defaultRatesFile(), planner);
    // Add Aircraft (D-242): a SimBrief screenshot read by Claude with the same key as the debrief (none in a scratch run,
    // where the values are typed in).
    AddAircraftModel addAircraft(planningDb, airline.catalog(), blocktime::defaultRatesFile(), claude);
    // The aircraft screen (D-241): New screenshot and Return.
    AircraftModel aircraft(planningDb, airline.catalog(), claude);
    PlannedFlightModel plannedFlight(planningDb, airlineIcao, planner, bridge, flightTarget, logDir,
                                     blocktime::defaultRatesFile());
    // The dashboard header (D-151, D-180): the planned flight, kept once settled; a summary already showing at startup
    // comes from the database. --demo shows its own.
    std::optional<DashboardHeader> liveHeader;
    if (!demo) {
        const auto settledFlight = [&planningDb](const QString& id) {
            return blocktime::flownFlightView(planningDb, id.toStdString());
        };
        liveHeader.emplace(bridge, plannedFlight, airlineIcao, QString::fromStdString(airline.airlineName()),
                           settledFlight);
    }
    DashboardHeader* header = demo ? demoHeader.get() : &*liveHeader;
    // The Pre-Flight panel (D-153): the planned flight with SayIntentions' ATIS and clearance; --demo's own flight.
    PreFlightModel preflight(bridge, demo ? static_cast<PlannedFlightDisplay&>(*demoPlanned) : plannedFlight);
    // The in-flight panel (D-194): block out to block in. SOULS counts the crew at rates.yaml's VIP multiple (D-203).
    const blocktime::Rates liveRates =
        blocktime::loadRates(blocktime::defaultRatesFile(), nullptr).value_or(blocktime::Rates{});
    InFlightModel inflight(bridge, demo ? static_cast<PlannedFlightDisplay&>(*demoPlanned) : plannedFlight,
                           static_cast<int>(liveRates.vipFlightAttendantsTimes), header);
    QObject::connect(&reviews, &ReviewWriter::reviewChanged, &flights, &FlightListModel::refreshDebrief);
    // A flight settled: the airplane moved, money was posted and Flights has a new row (D-113).
    QObject::connect(&plannedFlight, &PlannedFlightModel::settled, [&](const QString& logFlightId) {
        fleet.reset(fleetState());
        ledger.reset(airline.ledger(), airline.cash());
        flights.reload();
        reviews.onSettled(logFlightId, plannedFlight.takeSettledLines());
    });
    // A flight planned, cancelled or settled moves the Fleet cards' buttons; the rest of the planned flight's changes
    // (its plan, its estimate) don't, so the cards are rebuilt only when the planned airplane or leg changes.
    QObject::connect(&plannedFlight, &PlannedFlightModel::changed, [&] {
        const FleetState next = fleetState();
        if (next.plannedRegistration != fleet.plannedRegistration() || next.plannedLeg != fleet.plannedLeg()) {
            fleet.reset(next);
        }
    });
    // A plane leased: a new card, and its first month (and delivery) on Money.
    QObject::connect(&addAircraft, &AddAircraftModel::leased, [&] {
        fleet.reset(fleetState());
        ledger.reset(airline.ledger(), airline.cash());
    });
    // A plane returned (its refund on Money) or re-read (its card's layout may have changed).
    const auto fleetAndMoney = [&] {
        fleet.reset(fleetState());
        ledger.reset(airline.ledger(), airline.cash());
    };
    QObject::connect(&aircraft, &AircraftModel::returned, fleetAndMoney);
    QObject::connect(&aircraft, &AircraftModel::saved, fleetAndMoney);
    plannedFlight.checkFlown();  // a flight that ended while BlockTime was closed or crashed
    // Flights settle before leases (review 1.4): a flight that blocked in before a lease month is in the cash that
    // month is checked against. Each minute a flight still waiting to settle is tried again (review 1.2).
    const auto settleAirline = [&] {
        // A model reset rebuilds the views and scrolls them to the top, so only reset when something changed.
        if (!airline.tick(blocktime::utcToday())) return;
        fleet.reset(fleetState());
        ledger.reset(airline.ledger(), airline.cash());
    };
    settleAirline();
    QTimer airlineTick;
    airlineTick.setInterval(60'000);
    QObject::connect(&airlineTick, &QTimer::timeout, [&] {
        plannedFlight.retryPendingSettle();
        settleAirline();
    });
    airlineTick.start();
    QObject::connect(&newFlight, &NewFlightModel::planned, &plannedFlight,
                     [&plannedFlight](const QString& note) { plannedFlight.reload(note); });
    // SayIntentions at the gate (D-188): the planned flight's origin until block out, so the clearance shows before
    // pushback. The watcher itself waits for the sim and stops at block out.
    const auto preFlightOrigin = [&plannedFlight, &flightWatcher] {
        const QString state = plannedFlight.state();
        const bool atGate = state == "waitingForPlan" || state == "waitingForSim" || state == "simConnected";
        const auto& view = plannedFlight.view();
        flightWatcher.setPreFlightOrigin(atGate && view ? QString::fromStdString(view->origin) : QString());
    };
    QObject::connect(&plannedFlight, &PlannedFlightDisplay::changed, &flightWatcher, preFlightOrigin);
    preFlightOrigin();

    QQmlApplicationEngine engine;
    const QVariantMap props{{"recorder", QVariant::fromValue(&bridge)},
                            {"flights", QVariant::fromValue(&flights)},
                            {"reviews", QVariant::fromValue(&reviews)},
                            {"fleet", QVariant::fromValue(&fleet)},
                            {"addAircraft", QVariant::fromValue(&addAircraft)},
                            {"aircraft", QVariant::fromValue(&aircraft)},
                            {"ledger", QVariant::fromValue(&ledger)},
                            {"newFlight", QVariant::fromValue(&newFlight)},
                            {"plannedFlight", QVariant::fromValue(&plannedFlight)},
                            {"dashboard", QVariant::fromValue(header)},
                            {"preflight", QVariant::fromValue(&preflight)},
                            {"inflight", QVariant::fromValue(&inflight)},
                            {"airlineName", QString::fromStdString(airline.airlineName())},
                            {"startPage", startPage},
                            {"devMode", dev},
                            {"recording", !folders.scratch},
                            {"mapTileUrl", map.tileUrl},
                            {"mapCacheDir", map.cacheDir},
                            {"mapUserAgent", map.userAgent}};

    // Main.qml is created hidden; it's shown where it was last closed (D-249). A --grab keeps the QML size, and other
    // platforms (offscreen) have no Windows placement. A window that shows itself (hot reload's error stand-in) is
    // left alone.
    const bool rememberPlacement = grabFile.isEmpty() && QGuiApplication::platformName() == QStringLiteral("windows");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &engine, [rememberPlacement](QObject* root) {
        auto* window = qobject_cast<QQuickWindow*>(root);
        if (window && !window->isVisible()) {
            showRememberingPlacement(*window, blocktime::defaultWindowPlacementFile(), rememberPlacement);
        }
    });

#ifdef BLOCKTIME_QML_SOURCE_DIR
    // Debug: QML from the source folder, reloaded on save (D-055). A QML error leaves no window until the next good
    // save, so the app must not quit when its window goes away.
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QmlHotReload hotReload(engine, QStringLiteral(BLOCKTIME_QML_SOURCE_DIR), props);
    hotReload.load();
#else
    engine.setInitialProperties(props);
    engine.loadFromModule("BlockTime", "Main");
    if (engine.rootObjects().isEmpty()) {
        stopRecorder();
        return 1;
    }
#endif
    if (!grabFile.isEmpty()) grabAfterStartup(engine, grabFile);
    return QGuiApplication::exec();
}
