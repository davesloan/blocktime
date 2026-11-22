// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Qt UI test app (D-050): a Qt Quick window around the recorder. The recorder runs on its own thread and writes the
// same log as the console recorder (D-053); QML only displays what RecorderBridge exposes.
//
// Options:
//   --log-dir <folder>  as the console recorder
//   --demo              show made-up values and record nothing, to look at the UI without the sim

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string_view>
#include <thread>

#include "app_settings.h"
#include "event_log.h"
#include "flight_list_model.h"
#include "qml_hot_reload.h"
#include "recorder.h"
#include "recorder_bridge.h"
#include "single_instance.h"

namespace {

void copySimString(char* dst, std::size_t cap, std::string_view src) {
    std::memset(dst, 0, cap);
    std::memcpy(dst, src.data(), std::min(src.size(), cap - 1));
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
    bridge.applyBlockEvent(out);
}

struct MapSettings {
    QString tileUrl;  // holds the CARTO key: never print or log it
    QString cacheDir;
    QString userAgent;
};

// Map tiles (D-059): CARTO Dark Matter through the osm plugin's custom URL, which fills in %z/%x/%y even with the
// key after them (probed 2026-11-22, docs/notes/qt-setup.md). Each URL gets its own cache folder: CARTO answers a
// wrong key with a normal 200 "API KEY REQUIRED" tile, and the cache's file names don't include the URL, so those
// tiles would otherwise outlive a fixed key.
MapSettings mapSettings() {
    // CARTO's terms allow caching tiles for 30 days at most; this also clears folders of old keys.
    blocktime::pruneOldFiles(blocktime::defaultTileCacheDir(), blocktime::kTileMaxAge);

    MapSettings m;
    m.userAgent = QStringLiteral("BlockTime/" BLOCKTIME_VERSION " (Qt UI test; open source)");
    const std::string key = blocktime::loadAppSettings(blocktime::defaultSettingsFile()).cartoKey;
    if (key.empty()) return m;
    m.tileUrl = QStringLiteral("https://basemaps.cartocdn.com/rastertiles/dark_all/%z/%x/%y.png?key=") +
                QString::fromLatin1(QUrl::toPercentEncoding(QString::fromStdString(key)));
    const QString folder =
        QString::fromLatin1(QCryptographicHash::hash(m.tileUrl.toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
    m.cacheDir = QString::fromStdWString((blocktime::defaultTileCacheDir() / folder.toStdWString()).wstring());
    return m;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Basic is the Qt Quick Controls style meant to be fully re-skinned (D-054).
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("BlockTime");
    // Dark only: this also gives the window a dark Windows title bar (D-054).
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    std::filesystem::path logDir = blocktime::EventLog::defaultDir();
    bool demo = false;
    const QStringList args = QGuiApplication::arguments();
    for (qsizetype i = 1; i < args.size(); ++i) {
        if (args[i] == "--log-dir" && i + 1 < args.size()) {
            logDir = args[++i].toStdWString();
        } else if (args[i] == "--demo") {
            demo = true;
        }
    }

    // Declared before the thread, so they outlive it.
    RecorderBridge bridge;
    BridgeObserver observer(bridge);
    // Demo mode records nothing, so it doesn't take the recorder mutex (the console recorder can run beside it).
    std::optional<blocktime::SingleInstance> instance;
    if (!demo) instance.emplace();
    std::atomic<bool> stop{false};
    std::jthread worker;

    if (demo) {
        fillDemo(bridge);
    } else if (instance->acquired()) {
        worker = std::jthread([&] {
            const int result = blocktime::Recorder(logDir, observer).run(stop);
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

    // Flights come from the log (D-060). A block out or block in changes the list; the recorder writes the line to the
    // log before it tells the bridge, so the reload sees it.
    FlightListModel flights(logDir);
    QObject::connect(&bridge, &RecorderBridge::flightChanged, &flights, &FlightListModel::reload);

    QQmlApplicationEngine engine;
    const MapSettings map = mapSettings();
    const QVariantMap props{{"recorder", QVariant::fromValue(&bridge)},
                            {"flights", QVariant::fromValue(&flights)},
                            {"mapTileUrl", map.tileUrl},
                            {"mapCacheDir", map.cacheDir},
                            {"mapUserAgent", map.userAgent}};

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
    return QGuiApplication::exec();
}
