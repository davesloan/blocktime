// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The replay window (docs/replay.md, D-175): `BlockTime.exe --replay <flight>`, a second, read-only window that plays
// a settled flight on Live. No side menu: only Live, under the replay's bar.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property ReplaySession replay
    required property FlightListModel flights
    property int startPage: 0  // unused: the replay has Live only, but --page may be passed
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent

    width: Theme.windowWidth - Theme.sidebarWidth
    height: Theme.windowHeight
    minimumWidth: Theme.windowMinWidth - Theme.sidebarWidth
    minimumHeight: Theme.windowMinHeight
    visible: true
    title: "BlockTime replay · " + window.replay.title
    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header: what is replaying and the connection pill, which reads Replay
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight
            color: Theme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Theme.hairline
                color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.pagePadding
                anchors.rightMargin: Theme.pagePadding
                spacing: Theme.gap14

                Text {
                    Layout.fillWidth: true
                    text: "Replay · " + window.replay.title
                    elide: Text.ElideRight
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.DemiBold
                }

                StatusPill {
                    status: window.replay.bridge.connection
                }
            }
        }

        ReplayBar {
            Layout.fillWidth: true
            visible: window.replay.error === ""
            replay: window.replay
            onCloseRequested: window.close()
        }

        // The log is gone, or the flight isn't in it: say why instead of an empty Live.
        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.pagePadding
            visible: window.replay.error !== ""
            text: window.replay.error
            color: Theme.warn
            wrapMode: Text.WordWrap
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        LivePage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: window.replay.error === ""
            recorder: window.replay.bridge
            plannedFlight: window.replay.plannedFlight
            flights: window.flights
            reviews: null
            replaying: true
            mapTileUrl: window.mapTileUrl
            mapCacheDir: window.mapCacheDir
            mapUserAgent: window.mapUserAgent
        }
    }
}
