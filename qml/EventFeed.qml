// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the recorder reported, newest first: connection changes, block events, warnings.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Card {
    id: feed

    required property RecorderBridge recorder

    title: "Events"

    ListView {
        id: list

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: Theme.gap2
        boundsBehavior: Flickable.StopAtBounds
        model: feed.recorder.events

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Item {
            id: row

            required property string time
            required property string kind
            required property string text

            width: list.width
            implicitHeight: line.implicitHeight + Theme.gap10

            RowLayout {
                id: line

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: Theme.gap10
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.gap10

                Text {
                    Layout.alignment: Qt.AlignTop
                    text: row.time
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    font.features: { "tnum": 1 }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: Theme.feedDotOffset
                    Layout.preferredWidth: Theme.feedDotSize
                    Layout.preferredHeight: Theme.feedDotSize
                    radius: Theme.feedDotSize / 2
                    color: row.kind === "block" ? Theme.accent : row.kind === "warn" ? Theme.warn : Theme.border
                }

                Text {
                    Layout.fillWidth: true
                    text: row.text
                    color: row.kind === "info" ? Theme.textSecondary : Theme.textPrimary
                    wrapMode: Text.Wrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeFeed
                    font.weight: row.kind === "block" ? Font.DemiBold : Font.Normal
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: "Nothing yet"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }
    }
}
