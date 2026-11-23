// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Past flights (D-111): the settled flights from the database on the left, and the selected one's summary on the
// right, with its track on the dark map (D-059). Interrupted flights were discarded, so they aren't here (D-072).

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

    property var summary: ({})
    property var track

    function showCurrent() {
        page.summary = list.currentIndex >= 0 ? page.flights.summary(list.currentIndex) : ({})
        page.track = list.currentIndex >= 0 ? page.flights.track(list.currentIndex) : undefined
    }

    // The newest flight is selected whenever the list is (re)loaded, e.g. after a flight is settled. Tracks arrive
    // from the log a little later.
    Connections {
        target: page.flights

        function onModelReset() {
            list.currentIndex = page.flights.count > 0 ? 0 : -1
            page.showCurrent()
        }
        function onTracksChanged() { page.showCurrent() }
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
            onCurrentIndexChanged: page.showCurrent()
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
                text: "No flights yet"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }

        FlightSummary {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: list.currentIndex >= 0
            summary: page.summary
            track: page.track
            mapTileUrl: page.mapTileUrl
            mapCacheDir: page.mapCacheDir
            mapUserAgent: page.mapUserAgent
        }
    }

    Component.onCompleted: showCurrent()
}
