// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qml_hot_reload.h"

#ifdef BLOCKTIME_QML_SOURCE_DIR

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

QUrl QmlSourceInterceptor::intercept(const QUrl& url, DataType type) {
    if (type != QmlFile) return url;
    static const QString prefix = QStringLiteral("/qt/qml/BlockTime/");
    if (url.scheme() != QStringLiteral("qrc") || !url.path().startsWith(prefix)) return url;
    const QString name = url.path().mid(prefix.size());
    if (name.contains('/')) return url;
    const QString local = dir_ + '/' + name;
    return QFileInfo::exists(local) ? QUrl::fromLocalFile(local) : url;
}

QmlHotReload::QmlHotReload(QQmlApplicationEngine& engine, const QString& sourceDir, QVariantMap initialProperties,
                           QString root)
    : engine_(engine), dir_(sourceDir), props_(std::move(initialProperties)), root_(std::move(root)),
      interceptor_(sourceDir) {
    engine_.addUrlInterceptor(&interceptor_);
    // Editors often save in several writes (or replace the file); wait for them to settle.
    debounce_.setSingleShot(true);
    debounce_.setInterval(200);
    connect(&debounce_, &QTimer::timeout, this, [this] { load(); });
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] { debounce_.start(); });
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { debounce_.start(); });
    qInfo("QML hot reload: reading QML from %s", qUtf8Printable(dir_));
}

QmlHotReload::~QmlHotReload() { engine_.removeUrlInterceptor(&interceptor_); }

void QmlHotReload::load() {
    const QList<QObject*> roots = engine_.rootObjects();
    for (QObject* root : roots) delete root;
    // Theme is a singleton: without clearSingletons() its old values would survive the reload.
    engine_.clearSingletons();
    engine_.clearComponentCache();
    engine_.setInitialProperties(props_);
    engine_.loadFromModule("BlockTime", root_);

    if (!engine_.rootObjects().isEmpty()) {
        qInfo("QML hot reload: loaded");
    } else {
        qWarning("QML hot reload: %s didn't load. Fix the error above and save again.", qUtf8Printable(root_));
        // Without a window the app couldn't be closed except from Task Manager, and it would keep holding the
        // recorder mutex. A plain window that doesn't use the module stands in until the next good save.
        engine_.loadData(R"(
            import QtQuick
            Window {
                width: 560; height: 160; visible: true; color: "#0E1116"
                title: "BlockTime - QML error"
                Text {
                    anchors.fill: parent; anchors.margins: 20; wrapMode: Text.Wrap; color: "#E6EAF0"
                    font.pixelSize: 14
                    text: "Main.qml didn't load. The errors are in the debug output. Fix and save to reload, "
                          + "or close this window to quit (the recorder stops cleanly)."
                }
            })");
    }
    if (auto* window = engine_.rootObjects().isEmpty()
                           ? nullptr
                           : qobject_cast<QQuickWindow*>(engine_.rootObjects().constFirst())) {
        // quitOnLastWindowClosed is off in this mode (deleting the window for a reload mustn't quit), so closing
        // the window by hand quits here.
        connect(window, &QQuickWindow::closing, QCoreApplication::instance(), &QCoreApplication::quit);
    }
    watch();
}

void QmlHotReload::watch() {
    // A file replaced on save drops out of the watcher, so re-add everything each time.
    if (!watcher_.files().isEmpty()) watcher_.removePaths(watcher_.files());
    if (watcher_.directories().isEmpty()) watcher_.addPath(dir_);
    QStringList files;
    for (const QFileInfo& f : QDir(dir_).entryInfoList({"*.qml"}, QDir::Files)) files << f.absoluteFilePath();
    if (!files.isEmpty()) watcher_.addPaths(files);
}

#endif  // BLOCKTIME_QML_SOURCE_DIR
