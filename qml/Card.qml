// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A titled panel. Children go into a ColumnLayout, so they can use Layout.fillWidth / Layout.fillHeight.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Rectangle {
    id: card

    property string title
    default property alias content: body.data

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    border.width: Theme.borderWidth
    implicitWidth: column.implicitWidth + 2 * Theme.spacing
    implicitHeight: column.implicitHeight + 2 * Theme.spacing

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.gap12

        Text {
            visible: card.title !== ""
            text: card.title.toUpperCase()
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.DemiBold
            font.letterSpacing: Theme.labelLetterSpacing
        }

        ColumnLayout {
            id: body

            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gap8
        }
    }
}
