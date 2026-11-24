// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main call-to-action button: a re-skinned Basic style Button (D-054).

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic

Button {
    id: control

    leftPadding: Theme.spacing
    rightPadding: Theme.spacing
    topPadding: Theme.gap8
    bottomPadding: Theme.gap8
    opacity: enabled ? 1 : 0.45
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody
    font.weight: Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.textOnAccent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusSmall
        color: control.down ? Qt.darker(Theme.accent, 1.25)
             : control.hovered ? Qt.lighter(Theme.accent, 1.12)
             : Theme.accent
    }
}
