// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A yes/no value as colour (D-140): green when on, grey when off, empty before the first sample.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Rectangle {
    id: tile

    property string label
    property bool known: true
    property bool on: false
    property string onText: "ON"
    property string offText: "OFF"

    readonly property bool lit: tile.known && tile.on

    color: tile.lit ? Theme.goodSurface : Theme.surfaceRaised
    radius: Theme.radiusSmall
    border.color: tile.lit ? Theme.good : Theme.border
    border.width: 1
    implicitWidth: 150
    implicitHeight: 92

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 4

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

        Text {
            Layout.fillWidth: true
            text: !tile.known ? "—" : (tile.on ? tile.onText : tile.offText)
            color: tile.lit ? Theme.good : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeValue
            font.weight: Font.DemiBold
        }
    }
}
