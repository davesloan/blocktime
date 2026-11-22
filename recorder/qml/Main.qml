// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The shell: the side menu on the left, and on the right a header above the current page (D-062).
ApplicationWindow {
    id: window

    required property RecorderBridge recorder

    // 200 px wider than test 1, so the Live page keeps test 1's width beside the side menu.
    width: 1300
    height: 760
    minimumWidth: 1100
    minimumHeight: 600
    visible: true
    title: "BlockTime — Qt UI test 2"
    color: Theme.bg

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar

            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Header: page name, connection pill, Open log folder
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.headerHeight
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

                    Text {
                        Layout.fillWidth: true
                        text: sidebar.pageNames[sidebar.currentIndex]
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
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

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: sidebar.currentIndex

                LivePage {
                    recorder: window.recorder
                }

                FlightsPage {
                    recorder: window.recorder
                }

                FleetPage {
                    recorder: window.recorder
                }
            }
        }
    }
}
