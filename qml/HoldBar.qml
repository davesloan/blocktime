// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One hold in the hold split ("Forward 41,010"), as wide as its share of the holds. In a narrow window the label
// wraps to two lines instead of spilling out, and the bar never gets narrower than its longest word (D-248).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    id: bar

    required property string name
    required property string lb
    required property real share

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.preferredWidth: bar.share
    Layout.minimumWidth: Math.max(nameWidth.advanceWidth, lbWidth.advanceWidth) + 2 * Theme.gap4
    implicitHeight: Math.max(Theme.holdBarHeight, label.implicitHeight + 2 * Theme.gap4)
    radius: Theme.radiusSmall
    color: Theme.surfaceRaised
    border.color: Theme.cardBorder
    border.width: Theme.borderWidth

    TextMetrics {
        id: nameWidth

        font: label.font
        text: bar.name
    }

    TextMetrics {
        id: lbWidth

        font: label.font
        text: bar.lb
    }

    Text {
        id: label

        anchors.fill: parent
        anchors.margins: Theme.gap4
        text: bar.name + " " + bar.lb
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
    }
}
