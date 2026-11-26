// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small window in a steam gauge's face (the Mach number, the altimeter setting, the pounds of fuel), on its
// 220-unit face. A caption is a window without a frame.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    id: box

    property string text
    property int size: 15
    property bool caption: false  // grey letters and no frame

    radius: 2
    color: box.caption ? "transparent" : Theme.gaugeWindow
    border.color: box.caption ? "transparent" : Theme.gaugeWindowRing
    border.width: 1

    Text {
        anchors.centerIn: parent
        text: box.text
        color: box.caption ? Theme.gaugeCaption : Theme.gaugeNeedle
        font.family: box.caption ? Theme.fontFamily : Theme.monoFontFamily
        font.pixelSize: box.size
        font.letterSpacing: box.caption ? 1.5 : 0
    }
}
