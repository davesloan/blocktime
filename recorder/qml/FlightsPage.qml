// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Past flights from the event log, with the flown track on a map. For now only the map; the flight list comes
// with the flights model.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: page

    required property RecorderBridge recorder
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent

    RouteMap {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
        tileUrl: page.mapTileUrl
        cacheDir: page.mapCacheDir
        userAgent: page.mapUserAgent
    }
}
