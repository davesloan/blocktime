// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A dark map for flown tracks (D-059): Qt Location's osm plugin with CARTO Dark Matter raster tiles. Without a tile
// URL (no carto_key in settings.json) there's no map: the track is drawn on the dark background, scaled to fit.
//
// CARTO's terms: the credit line stays visible; our own User-Agent; tiles cached on this PC only, at most 30 days
// (the app prunes the cache at startup); no bulk download (prefetching is limited to one neighbouring zoom level).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtLocation
import QtPositioning

Rectangle {
    id: root

    // Full tile URL with %z/%x/%y and the key, or "" for no tiles. Never shown or logged.
    required property string tileUrl
    required property string cacheDir
    required property string userAgent
    // The track to draw: a geopath from FlightListModel.track(), or undefined for none.
    property var track

    readonly property bool hasTiles: tileUrl !== ""
    readonly property var trackPoints: track ? track.path : []
    readonly property bool hasTrack: trackPoints.length > 0
    readonly property Map map: mapLoader.item ? (mapLoader.item as MapView).map : null

    readonly property real maxFitZoom: 14  // about 2 km across: an airport and its surroundings

    // Frames the whole track. A track that never left the gate is shown at maxFitZoom around its middle rather than
    // zoomed in past the tiles' detail. fitViewportToGeoShape applies after this returns, so the cap for larger
    // tracks is applied a moment later.
    function fitTrack() {
        // Read the track itself: in onTrackChanged the trackPoints binding may not have caught up yet.
        const pts = track ? track.path : []
        if (!map || pts.length === 0) return
        let minLat = 90, maxLat = -90, minLon = 180, maxLon = -180
        for (const p of pts) {
            minLat = Math.min(minLat, p.latitude)
            maxLat = Math.max(maxLat, p.latitude)
            minLon = Math.min(minLon, p.longitude)
            maxLon = Math.max(maxLon, p.longitude)
        }
        const sw = QtPositioning.coordinate(minLat, minLon), ne = QtPositioning.coordinate(maxLat, maxLon)
        if (sw.distanceTo(ne) < 2000) {
            map.center = QtPositioning.coordinate((minLat + maxLat) / 2, (minLon + maxLon) / 2)
            map.zoomLevel = maxFitZoom
            return
        }
        map.fitViewportToGeoShape(track, 48)
        Qt.callLater(() => { if (root.map && root.map.zoomLevel > root.maxFitZoom) root.map.zoomLevel = root.maxFitZoom })
    }

    onTrackChanged: fitTrack()
    onMapChanged: fitTrack()

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

            MapPolyline {
                id: line

                visible: root.trackPoints.length > 1
                line.width: 3
                line.color: Theme.accent
                path: root.trackPoints
            }

            MapQuickItem {
                id: startDot

                visible: root.hasTrack
                coordinate: root.hasTrack ? root.trackPoints[0] : QtPositioning.coordinate()
                anchorPoint.x: 7
                anchorPoint.y: 7
                sourceItem: TrackDot { tone: Theme.good }
            }

            MapQuickItem {
                id: endDot

                visible: root.hasTrack
                coordinate: root.hasTrack ? root.trackPoints[root.trackPoints.length - 1] : QtPositioning.coordinate()
                anchorPoint.x: 7
                anchorPoint.y: 7
                sourceItem: TrackDot { tone: Theme.accent }
            }

            Component.onCompleted: {
                // The custom URL map type is the last one the plugin offers.
                map.activeMapType = map.supportedMapTypes[map.supportedMapTypes.length - 1]
                map.addMapItem(line)
                map.addMapItem(startDot)
                map.addMapItem(endDot)
            }
        }
    }

    // No tiles: the track alone, scaled to fit (equirectangular, longitude scaled by cos of the mid latitude).
    Item {
        id: plain

        readonly property real margin: 48
        readonly property var points: {
            const pts = root.trackPoints
            if (root.hasTiles || pts.length === 0 || width <= 2 * margin || height <= 2 * margin) return []
            let minLat = 90, maxLat = -90, minLon = 180, maxLon = -180
            for (const p of pts) {
                minLat = Math.min(minLat, p.latitude)
                maxLat = Math.max(maxLat, p.latitude)
                minLon = Math.min(minLon, p.longitude)
                maxLon = Math.max(maxLon, p.longitude)
            }
            const k = Math.cos((minLat + maxLat) / 2 * Math.PI / 180)
            const w = width - 2 * margin, h = height - 2 * margin
            const spanX = Math.max((maxLon - minLon) * k, 1e-6), spanY = Math.max(maxLat - minLat, 1e-6)
            const s = Math.min(w / spanX, h / spanY)
            const ox = margin + (w - spanX * s) / 2, oy = margin + (h - spanY * s) / 2
            return pts.map(p => Qt.point(ox + (p.longitude - minLon) * k * s, oy + (maxLat - p.latitude) * s))
        }

        anchors.fill: parent
        visible: points.length > 0

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: Theme.accent
                strokeWidth: 3
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathPolyline { path: plain.points }
            }
        }

        TrackDot {
            x: plain.points.length ? plain.points[0].x - width / 2 : 0
            y: plain.points.length ? plain.points[0].y - height / 2 : 0
            tone: Theme.good
        }

        TrackDot {
            x: plain.points.length ? plain.points[plain.points.length - 1].x - width / 2 : 0
            y: plain.points.length ? plain.points[plain.points.length - 1].y - height / 2 : 0
            tone: Theme.accent
        }
    }

    // No key: say why there's no map.
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing
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
