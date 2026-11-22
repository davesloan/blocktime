// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A dark map for flown tracks (D-059): Qt Location's osm plugin with CARTO Dark Matter raster tiles. Without a tile
// URL (no carto_key in settings.json) there's no map, only the dark background and a note.
//
// CARTO's terms: the credit line stays visible; our own User-Agent; tiles cached on this PC only, at most 30 days
// (the app prunes the cache at startup); no prefetching beyond what the view needs.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtLocation
import QtPositioning

Rectangle {
    id: root

    // Full tile URL with %z/%x/%y and the key, or "" for no tiles. Never shown or logged.
    required property string tileUrl
    required property string cacheDir
    required property string userAgent

    readonly property bool hasTiles: tileUrl !== ""
    readonly property Map map: mapLoader.item ? (mapLoader.item as MapView).map : null

    color: Theme.bg
    clip: true

    Loader {
        id: mapLoader

        anchors.fill: parent
        active: root.hasTiles

        sourceComponent: MapView {
            id: view

            map.color: Theme.bg
            map.copyrightsVisible: false  // replaced by the credit line below, in the theme's colours
            map.center: QtPositioning.coordinate(32.733, -117.19)  // KSAN until a track is shown
            map.zoomLevel: 10
            map.plugin: Plugin {
                name: "osm"

                PluginParameter { name: "osm.useragent"; value: root.userAgent }
                PluginParameter { name: "osm.mapping.custom.host"; value: root.tileUrl }
                PluginParameter { name: "osm.mapping.custom.mapcopyright"; value: "CARTO" }
                PluginParameter { name: "osm.mapping.custom.datacopyright"; value: "OpenStreetMap contributors" }
                // Don't ask Qt's own server for its provider list: only the custom URL is used.
                PluginParameter { name: "osm.mapping.providersrepository.disabled"; value: true }
                PluginParameter { name: "osm.mapping.cache.directory"; value: root.cacheDir }
                PluginParameter { name: "osm.mapping.cache.disk.cost_strategy"; value: "bytesize" }
                PluginParameter { name: "osm.mapping.cache.disk.size"; value: 104857600 }     // 100 MB
                PluginParameter { name: "osm.mapping.cache.memory.cost_strategy"; value: "bytesize" }
                PluginParameter { name: "osm.mapping.cache.memory.size"; value: 16777216 }    // 16 MB
                PluginParameter { name: "osm.mapping.cache.texture.cost_strategy"; value: "bytesize" }
                PluginParameter { name: "osm.mapping.cache.texture.size"; value: 33554432 }   // 32 MB
                PluginParameter { name: "osm.mapping.prefetching_style"; value: "OneNeighbourLayer" }
            }

            // The custom URL map type is the last one the plugin offers.
            Component.onCompleted: map.activeMapType = map.supportedMapTypes[map.supportedMapTypes.length - 1]
        }
    }

    // No key: say why there's no map.
    Text {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.spacing
        visible: !root.hasTiles
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: "No map key: add carto_key to %LOCALAPPDATA%\\BlockTime\\settings.json"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
    }

    // CARTO's required credit line, bottom right.
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.hasTiles
        width: credit.implicitWidth + 12
        height: credit.implicitHeight + 6
        color: Theme.mapCreditBg
        topLeftRadius: Theme.radiusSmall

        Text {
            id: credit

            anchors.centerIn: parent
            textFormat: Text.StyledText
            text: "© <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> contributors, "
                  + "© <a href=\"https://carto.com/attributions\">CARTO</a>"
            color: Theme.textSecondary
            linkColor: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }
}
