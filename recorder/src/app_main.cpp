// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Qt UI test app (D-050): a Qt Quick window around the recorder.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>

int main(int argc, char* argv[]) {
    // Basic is the Qt Quick Controls style meant to be fully re-skinned (D-054).
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("BlockTime");
    // Dark only: this also gives the window a dark Windows title bar (D-054).
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    QQmlApplicationEngine engine;
    engine.loadFromModule("BlockTime", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    return QGuiApplication::exec();
}
