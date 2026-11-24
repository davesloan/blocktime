// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One live value: a small caps label over a large number with its unit.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Rectangle {
    id: tile

    property string label
    property string value
    property string unit
    property color accent: Theme.textPrimary

    color: Theme.surfaceRaised
    radius: Theme.radiusSmall
    border.color: Theme.border
    border.width: Theme.borderWidth
    implicitWidth: Theme.tileWidth
    implicitHeight: Theme.tileHeight

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.gap14
        anchors.rightMargin: Theme.gap14
        spacing: Theme.gap4

        Text {
            Layout.fillWidth: true
            text: tile.label.toUpperCase()
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.DemiBold
            font.letterSpacing: Theme.labelLetterSpacing
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap6

            Text {
                text: tile.value
                color: tile.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeValue
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }  // tabular figures, so changing digits don't jitter
                Layout.alignment: Qt.AlignBaseline
            }

            Text {
                visible: tile.unit !== "" && tile.value !== "—"
                text: tile.unit
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                Layout.alignment: Qt.AlignBaseline
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }
}
