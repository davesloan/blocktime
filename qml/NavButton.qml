// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One entry in the side menu: an icon glyph and a label. The selected entry gets an accent bar on its left (D-062).

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

AbstractButton {
    id: button

    // A Segoe Fluent Icons code point, e.g. "".
    property string glyph
    property bool selected

    implicitHeight: Theme.navItemHeight
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radiusSmall
        color: button.selected ? Theme.surfaceRaised : button.hovered ? Theme.surfaceHover : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.accentBarWidth
            height: parent.height - Theme.spacing
            radius: Theme.accentBarWidth / 2
            color: Theme.accent
            visible: button.selected
        }
    }

    contentItem: RowLayout {
        spacing: Theme.gap12

        Text {
            Layout.leftMargin: Theme.gap14
            text: button.glyph
            color: button.selected ? Theme.accent : Theme.textSecondary
            font.family: Theme.iconFontFamily
            font.pixelSize: Theme.fontSizeIcon
        }

        Text {
            Layout.fillWidth: true
            text: button.text
            color: button.selected ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: button.selected ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
        }
    }
}
