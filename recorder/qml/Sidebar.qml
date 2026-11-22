// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The side menu (D-062): the brand mark and one NavButton per page. currentIndex is the page shown.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Rectangle {
    id: sidebar

    // 0 = Live, 1 = Flights, 2 = Fleet
    property int currentIndex: 0
    // Page names, in the same order, for the header.
    readonly property var pageNames: entries.map(e => e.text)

    // Glyphs are Segoe Fluent Icons code points: SpeedHigh, History, Airplane.
    readonly property var entries: [
        { glyph: "", text: "Live" },
        { glyph: "", text: "Flights" },
        { glyph: "", text: "Fleet" }
    ]

    implicitWidth: Theme.sidebarWidth
    color: Theme.surface

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 4

        // Brand
        RowLayout {
            Layout.preferredHeight: Theme.headerHeight - 12
            Layout.leftMargin: 8
            Layout.bottomMargin: 8
            spacing: 12

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
                    text: "Qt UI test 2"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                }
            }
        }

        Repeater {
            model: sidebar.entries

            NavButton {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                glyph: modelData.glyph
                text: modelData.text
                selected: sidebar.currentIndex === index
                onClicked: sidebar.currentIndex = index
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
