// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Past flights from the event log (D-060): a list on the left, and for the selected flight its numbers and its
// flown track on the dark map (D-059).

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property RecorderBridge recorder
    required property FlightListModel flights
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent

    // The selected flight's roles, read through the list's current item.
    readonly property var current: list.currentItem ? (list.currentItem as FlightCard) : null

    function grouped(x: real): string {
        return String(Math.round(x)).replace(/\B(?=(\d{3})+(?!\d))/g, ",")
    }

    // The newest flight is selected whenever the list is (re)loaded, e.g. after a block in.
    Connections {
        target: page.flights

        function onModelReset() {
            list.currentIndex = page.flights.count > 0 ? 0 : -1
            page.showTrack()
        }
    }

    function showTrack() {
        routeMap.track = list.currentIndex >= 0 ? page.flights.track(list.currentIndex) : undefined
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.spacing

        // Flight list
        ListView {
            id: list

            Layout.preferredWidth: 340
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: page.flights
            currentIndex: page.flights.count > 0 ? 0 : -1
            onCurrentIndexChanged: page.showTrack()
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: FlightCard {
                width: ListView.view.width - 10
                selected: ListView.isCurrentItem
                onClicked: list.currentIndex = index
            }

            Text {
                anchors.centerIn: parent
                visible: page.flights.count === 0
                text: "No flights in the log yet"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }

        // Selected flight
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 12
                rowSpacing: 12
                visible: page.current !== null

                StatTile {
                    Layout.fillWidth: true
                    label: "Block out"
                    value: page.current ? page.current.outText : "—"
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "Block in"
                    value: page.current && page.current.inText !== "" ? page.current.inText : "—"
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "Block time"
                    value: page.current ? page.current.blockText : "—"
                    accent: Theme.accent
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "Fuel used"
                    value: page.current && page.current.fuelUsedLb >= 0 ? page.grouped(page.current.fuelUsedLb) : "—"
                    unit: "lb"
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "Distance flown"
                    value: page.current ? page.current.distanceNm.toFixed(1) : "—"
                    unit: "nm"
                }
                StatTile {
                    Layout.fillWidth: true
                    label: "Highest altitude"
                    value: page.current ? page.grouped(page.current.maxAltFt) : "—"
                    unit: "ft"
                }
            }

            RouteMap {
                id: routeMap

                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radius
                border.color: Theme.border
                border.width: 1
                tileUrl: page.mapTileUrl
                cacheDir: page.mapCacheDir
                userAgent: page.mapUserAgent
            }
        }
    }

    Component.onCompleted: showTrack()
}
