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
    readonly property color textMuted: "#5B6573"  // stages not reached yet

    // Meaning
    readonly property color accent: "#3B82F6"
    // Text on an accent fill. (Not "onAccent": QML reads on<Capital> as a signal handler.)
    readonly property color textOnAccent: "#FFFFFF"
    readonly property color good: "#22C55E"
    readonly property color warn: "#F59E0B"
    readonly property color bad: "#EF4444"
    readonly property color warnSurface: "#2A2112"  // warn mixed into bg: the Not recording banner
    readonly property color goodSurface: "#132A1C"  // good mixed into bg: a status tile that's on (D-140)

    // Shape
    readonly property int radius: 10
    readonly property int radiusSmall: 8
    readonly property int spacing: 16
    readonly property int pagePadding: 20
    readonly property int headerHeight: 64
    readonly property int sidebarWidth: 200
    readonly property int navItemHeight: 40
    readonly property int stageDotSize: 8      // Flight progress: the marker before each stage
    readonly property int stageTimeWidth: 104  // Flight progress: "22:51Z – 22:54Z"

    // Spacing scale: every gap, margin and padding below Theme.spacing (16) comes from here
    readonly property int gap2: 2
    readonly property int gap4: 4
    readonly property int gap6: 6
    readonly property int gap8: 8
    readonly property int gap10: 10
    readonly property int gap12: 12
    readonly property int gap14: 14

    // Lines
    readonly property int borderWidth: 1
    readonly property int hairline: 1          // separators
    readonly property int accentBarWidth: 3    // the selected nav item's and fleet row's bar
    readonly property int routeLineWidth: 3    // the flight's track on the map

    // Controls
    readonly property int controlHeight: 36    // buttons and text fields
    readonly property int chipHeight: 32       // New Flight's kind chips, the collapse button
    readonly property int pillHeight: 28       // the connection pill
    readonly property int badgeHeight: 22      // the fleet's status badges
    readonly property int logoSize: 30         // the sidebar's logo
    readonly property int shortFieldWidth: 160 // New Flight's flight number
    readonly property int dialogWidth: 600     // New Flight

    // Dots
    readonly property int feedDotSize: 7       // the event log's markers
    readonly property int feedDotOffset: 5     // lines them up with the first text line
    readonly property int trackDotSize: 14     // the aircraft on the map
    readonly property int trackDotBorder: 3

    // Tiles, tables and columns
    readonly property int tileWidth: 150       // StatTile, StatusTile
    readonly property int tileHeight: 92
    readonly property int tableHeaderHeight: 36
    readonly property int tableRowHeight: 44
    readonly property int ledgerRowHeight: 40
    readonly property int ledgerDateWidth: 110
    readonly property int cashTileWidth: 360
    readonly property int sideColumnWidth: 340 // Live's Flight progress, the Flights list, the summary's money (D-139)
    readonly property int liveFeedWidth: 300   // Live's event log; the planned flight's side column
    readonly property int summaryHeight: 520
    readonly property int summaryMapMinHeight: 240
    readonly property int checkMarkWidth: 18     // the debrief's ✓ / ✗ column
    readonly property int checkPointsWidth: 36   // the debrief's "−6" column

    // Window
    readonly property int windowWidth: 1300
    readonly property int windowHeight: 760
    readonly property int windowMinWidth: 1100
    readonly property int windowMinHeight: 600

    // Type
    readonly property string fontFamily: "Segoe UI Variable"
    readonly property int fontSizeLabel: 11  // small caps labels above values
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeFeed: 13   // the event log
    readonly property int fontSizeBody: 14
    readonly property int fontSizeTitle: 18
    readonly property int fontSizeValue: 28
    readonly property int fontSizeClock: 44
    readonly property real labelLetterSpacing: 1.2
    readonly property string monoFontFamily: "Consolas"  // the route box

    // Icons: Windows 11 system icon font, nothing to bundle (D-062)
    readonly property string iconFontFamily: "Segoe Fluent Icons"
    readonly property int fontSizeIcon: 16
}
