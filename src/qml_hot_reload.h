// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Debug builds only (D-055): QML is read from the source folder instead of the copy compiled into the exe, and the
// window reloads when a .qml file is saved, so a UI change shows without a rebuild. The recorder thread and the
// bridge keep running through a reload.

#pragma once

#ifdef BLOCKTIME_QML_SOURCE_DIR

#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlAbstractUrlInterceptor>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class QQmlApplicationEngine;

// Maps the module's qrc:/qt/qml/BlockTime/<name>.qml to <sourceDir>/<name>.qml.
class QmlSourceInterceptor : public QQmlAbstractUrlInterceptor {
public:
    explicit QmlSourceInterceptor(QString sourceDir) : dir_(std::move(sourceDir)) {}
    QUrl intercept(const QUrl& url, DataType type) override;

private:
    QString dir_;
};

class QmlHotReload : public QObject {
public:
    // `root` is the module's root component: Main, or ReplayWindow for --replay (D-175).
    QmlHotReload(QQmlApplicationEngine& engine, const QString& sourceDir, QVariantMap initialProperties,
                 QString root = QStringLiteral("Main"));
    ~QmlHotReload() override;

    // Replaces any loaded window with a fresh Main.qml. On a QML error there is no window until the next good save;
    // the errors go to the debug output.
    void load();

private:
    void watch();

    QQmlApplicationEngine& engine_;
    QString dir_;
    QVariantMap props_;
    QString root_;
    QmlSourceInterceptor interceptor_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
};

#endif  // BLOCKTIME_QML_SOURCE_DIR
