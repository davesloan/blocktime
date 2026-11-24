// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The quieter button: an outline on the surface, for actions next to a PrimaryButton (Cancel, Copy route).

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic

Button {
    id: control

    property color tone: Theme.textPrimary

    leftPadding: Theme.gap14
    rightPadding: Theme.gap14
    topPadding: Theme.gap8
    bottomPadding: Theme.gap8
    opacity: enabled ? 1 : 0.45
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeBody
    font.weight: Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.tone
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusSmall
        color: control.down ? Theme.surfaceRaised : control.hovered ? Theme.surfaceHover : "transparent"
        border.color: Theme.border
        border.width: Theme.borderWidth
    }
}
