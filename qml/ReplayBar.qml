// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The replay's controls (docs/replay.md, D-175): the speed, Pause / Resume and Close; at block in, Replay again and
// Close.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: bar

    required property ReplaySession replay

    signal closeRequested()

    implicitHeight: Theme.headerHeight
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
        spacing: Theme.gap8

        Text {
            Layout.rightMargin: Theme.gap14
            text: bar.replay.finished ? "■ REPLAY ENDED" : bar.replay.paused ? "❚❚ PAUSED" : "▶ REPLAY"
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
        }

        Repeater {
            model: [1, 2, 3, 10]

            delegate: SecondaryButton {
                required property int modelData

                visible: !bar.replay.finished
                text: modelData + "×"
                tone: bar.replay.speed === modelData ? Theme.accent : Theme.textPrimary
                onClicked: bar.replay.setSpeed(modelData)
            }
        }

        SecondaryButton {
            Layout.leftMargin: Theme.gap14
            visible: !bar.replay.finished
            text: bar.replay.paused ? "Resume" : "Pause"
            onClicked: bar.replay.setPaused(!bar.replay.paused)
        }

        PrimaryButton {
            visible: bar.replay.finished
            text: "Replay again"
            onClicked: bar.replay.restart()
        }

        Item { Layout.fillWidth: true }

        SecondaryButton {
            text: "Close"
            onClicked: bar.closeRequested()
        }
    }
}
