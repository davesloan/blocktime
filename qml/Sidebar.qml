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

    // 0 = Live, 1 = Flights, 2 = Fleet, 3 = Money
    property int currentIndex: 0
    // The airline's name under the brand; "" shows none.
    property string airlineName
    // Page names, in the same order, for the header.
    readonly property var pageNames: entries.map(e => e.text)

    // Glyphs are Segoe Fluent Icons code points: SpeedHigh, History, Airplane, Money (E8C7).
    readonly property var entries: [
        { glyph: "", text: "Live" },
        { glyph: "", text: "Flights" },
        { glyph: "", text: "Fleet" },
        { glyph: "", text: "Money" }
    ]

    implicitWidth: Theme.sidebarWidth
    color: Theme.surface

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: Theme.hairline
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap12
        spacing: Theme.gap4

        // Brand
        RowLayout {
            Layout.preferredHeight: Theme.headerHeight - 12
            Layout.leftMargin: Theme.gap8
            Layout.bottomMargin: Theme.gap8
            spacing: Theme.gap12

            Rectangle {
                Layout.preferredWidth: Theme.logoSize
                Layout.preferredHeight: Theme.logoSize
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
                    visible: sidebar.airlineName !== ""
                    text: sidebar.airlineName
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
                Text {
                    text: "v" + Qt.application.version
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
