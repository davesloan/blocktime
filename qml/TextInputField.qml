// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A one-line text field in the app's look (New Flight's destination and flight number).

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic

TextField {
    id: field

    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        implicitHeight: 36
        radius: Theme.radiusSmall
        color: Theme.bg
        border.color: field.activeFocus ? Theme.accent : Theme.border
        border.width: 1
    }
}
