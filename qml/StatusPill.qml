// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The recorder's connection state as a coloured pill. (The plan called the property `state`, but every Item already
// has a `state` property, so it is `status` here.)

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    id: pill

    // "connected", "starting", "waiting" or "blocked", as RecorderBridge.connection
    property string status

    readonly property color tone: status === "connected" ? Theme.good
                                : status === "starting" ? Theme.warn
                                : status === "blocked" ? Theme.bad
                                : Theme.textSecondary
    readonly property string label: status === "connected" ? "Connected"
                                  : status === "starting" ? "Sim starting"
                                  : status === "blocked" ? "Recorder already running"
                                  : "Waiting for sim"

    implicitWidth: row.implicitWidth + 24
    implicitHeight: Theme.pillHeight
    radius: height / 2
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.12)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.35)
    border.width: Theme.borderWidth

    Row {
        id: row

        anchors.centerIn: parent
        spacing: Theme.gap8

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.stageDotSize
            height: Theme.stageDotSize
            radius: Theme.stageDotSize / 2
            color: pill.tone
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: pill.label
            color: pill.tone
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
        }
    }
}
