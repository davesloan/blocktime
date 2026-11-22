// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property RecorderBridge recorder

    width: 1100
    height: 720
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "BlockTime — Qt UI test 1"
    color: Theme.bg

    header: Rectangle {
        implicitHeight: 64
        color: Theme.surface

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.pagePadding
            anchors.rightMargin: Theme.pagePadding
            spacing: 14

            // Brand mark
            Rectangle {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                radius: Theme.radiusSmall
                color: Theme.accent

                Text {
                    anchors.centerIn: parent
                    text: "B"
                    color: Theme.textOnAccent
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.Bold
                }
            }

            Column {
                Text {
                    text: "BlockTime"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Qt UI test 1"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                }
            }

            Item {
                Layout.fillWidth: true
            }

            StatusPill {
                status: window.recorder.connection
            }

            PrimaryButton {
                text: "Open log folder"
                enabled: window.recorder.logDir !== ""
                onClicked: window.recorder.openLogFolder()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.spacing

        FlightPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            recorder: window.recorder
        }

        EventFeed {
            Layout.fillHeight: true
            Layout.preferredWidth: 360
            recorder: window.recorder
        }
    }
}
