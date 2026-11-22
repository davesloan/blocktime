// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one place colours, sizes and fonts are written (D-054). Dark only.

pragma Singleton

import QtQuick

QtObject {
    // Surfaces, darkest first
    readonly property color bg: "#0E1116"
    readonly property color surface: "#161B22"
    readonly property color surfaceRaised: "#1C222B"
    readonly property color surfaceHover: "#1A1F27"  // between surface and surfaceRaised
    readonly property color border: "#262D37"
    readonly property color mapCreditBg: "#B30E1116"  // bg at 70%, behind the map's credit line

    // Text
    readonly property color textPrimary: "#E6EAF0"
    readonly property color textSecondary: "#8B95A3"

    // Meaning
    readonly property color accent: "#3B82F6"
    // Text on an accent fill. (Not "onAccent": QML reads on<Capital> as a signal handler.)
    readonly property color textOnAccent: "#FFFFFF"
    readonly property color good: "#22C55E"
    readonly property color warn: "#F59E0B"
    readonly property color bad: "#EF4444"

    // Shape
    readonly property int radius: 10
    readonly property int radiusSmall: 8
    readonly property int spacing: 16
    readonly property int pagePadding: 20
    readonly property int headerHeight: 64
    readonly property int sidebarWidth: 200
    readonly property int navItemHeight: 40

    // Type
    readonly property string fontFamily: "Segoe UI Variable"
    readonly property int fontSizeLabel: 11  // small caps labels above values
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeBody: 14
    readonly property int fontSizeTitle: 18
    readonly property int fontSizeValue: 28
    readonly property int fontSizeClock: 44
    readonly property real labelLetterSpacing: 1.2

    // Icons: Windows 11 system icon font, nothing to bundle (D-062)
    readonly property string iconFontFamily: "Segoe Fluent Icons"
    readonly property int fontSizeIcon: 16
}
