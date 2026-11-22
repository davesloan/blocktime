// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Qt UI test app (D-050): a Qt Quick window around the recorder. The recorder runs on its own thread and writes the
// same log as the console recorder (D-053); QML only displays what RecorderBridge exposes.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>

#include <atomic>
#include <filesystem>
#include <thread>

#include "event_log.h"
#include "recorder.h"
#include "recorder_bridge.h"
#include "single_instance.h"

int main(int argc, char* argv[]) {
    // Basic is the Qt Quick Controls style meant to be fully re-skinned (D-054).
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("BlockTime");
    // Dark only: this also gives the window a dark Windows title bar (D-054).
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    // Same option as the console recorder.
    std::filesystem::path logDir = blocktime::EventLog::defaultDir();
    const QStringList args = QGuiApplication::arguments();
    for (qsizetype i = 1; i < args.size(); ++i) {
        if (args[i] == "--log-dir" && i + 1 < args.size()) logDir = args[++i].toStdWString();
    }

    // Declared before the thread, so they outlive it.
    RecorderBridge bridge;
    BridgeObserver observer(bridge);
    const blocktime::SingleInstance instance;
    std::atomic<bool> stop{false};
    std::jthread worker;

    if (instance.acquired()) {
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

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"recorder", QVariant::fromValue(&bridge)}});
    engine.loadFromModule("BlockTime", "Main");
    if (engine.rootObjects().isEmpty()) {
        stopRecorder();
        return 1;
    }
    return QGuiApplication::exec();
}
