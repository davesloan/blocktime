// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A steam gauge's ticks and numerals on its 220-unit face. Angles in degrees clockwise from twelve o'clock. Drawn
// once: only a needle moves.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: gaugeScale

    property var ticks: []   // [{deg, len, w}]
    property var labels: []  // [{deg, r, text, size}]

    width: 220
    height: 220

    Repeater {
        model: gaugeScale.ticks

        Item {
            id: tick

            required property var modelData

            x: 110
            y: 110
            rotation: tick.modelData.deg

            Rectangle {
                x: -tick.modelData.w / 2
                y: -92
                width: tick.modelData.w
                height: tick.modelData.len
                color: Theme.gaugeTick
            }
        }
    }

    Repeater {
        model: gaugeScale.labels

        Text {
            id: numeral

            required property var modelData
            readonly property real rad: numeral.modelData.deg * Math.PI / 180

            x: 110 + numeral.modelData.r * Math.sin(rad) - width / 2
            y: 110 - numeral.modelData.r * Math.cos(rad) - height / 2
            text: numeral.modelData.text
            color: Theme.gaugeNumeral
            font.family: "Bahnschrift"
            font.pixelSize: numeral.modelData.size
            font.weight: Font.DemiBold
        }
    }
}
