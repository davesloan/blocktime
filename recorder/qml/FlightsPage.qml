// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Past flights from the event log, with the flown track on a map. Placeholder until the flights model exists.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: page

    required property RecorderBridge recorder

    Text {
        anchors.centerIn: parent
        text: "Flights: not built yet"
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
    }
}
