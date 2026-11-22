// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airline's airframes, read from SQLite. Placeholder until the fleet model exists.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: page

    required property RecorderBridge recorder

    Text {
        anchors.centerIn: parent
        text: "Fleet: not built yet"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
    }
}
