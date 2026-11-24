// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// "Not recording" with the reason, across the top of the Live page while the sim doesn't match the planned flight
// (design.md §13.4, D-088). Hidden when there's nothing to say.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Rectangle {
    id: banner

    required property string text

    visible: text !== ""
    implicitHeight: row.implicitHeight + 2 * Theme.gap12
    radius: Theme.radiusSmall
    color: Theme.warnSurface
    border.color: Theme.warn
    border.width: Theme.borderWidth

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: Theme.gap14
        anchors.rightMargin: Theme.gap14
        spacing: Theme.gap10

        Text {
            text: ""  // Warning
            color: Theme.warn
            font.family: Theme.iconFontFamily
            font.pixelSize: Theme.fontSizeIcon
        }

        Text {
            Layout.fillWidth: true
            text: banner.text
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }
    }
}
