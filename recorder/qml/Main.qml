// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic

ApplicationWindow {
    id: window

    width: 1100
    height: 720
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "BlockTime — Qt UI test 1"
    color: Theme.bg

    Text {
        x: Theme.spacing
        y: Theme.spacing
        text: "BlockTime"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTitle
        font.weight: Font.DemiBold
    }
}
