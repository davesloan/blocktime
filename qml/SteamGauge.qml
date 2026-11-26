// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A steam gauge (D-179): a metal bezel, a black face and a faint glare on the glass, with an engraved label under
// it. Children are drawn on a 220-unit face (the mockup's units, centre 110, ticks out to 92) scaled to
// Theme.gaugeSize, between the face and the glass; the hub goes on top of them.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: gauge

    property string label
    default property alias content: face.data

    implicitWidth: Theme.gaugeSize
    implicitHeight: Theme.gaugeSize + Theme.gap2 + labelText.implicitHeight

    Item {
        id: dial

        width: 220
        height: 220
        scale: Theme.gaugeSize / 220
        transformOrigin: Item.TopLeft

        Rectangle {
            x: 4
            y: 4
            width: 212
            height: 212
            radius: width / 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Theme.gaugeBezelLight }
                GradientStop { position: 0.5; color: Theme.gaugeBezelDark }
                GradientStop { position: 1; color: Theme.gaugeBezelLight }
            }
        }
        Rectangle {
            x: 12
            y: 12
            width: 196
            height: 196
            radius: width / 2
            color: Theme.gaugeFace
            border.color: "black"
            border.width: 2
        }

        Item {
            id: face

            anchors.fill: parent
        }

        // The hub over the needles.
        Rectangle {
            x: 110 - 8
            y: 110 - 8
            width: 16
            height: 16
            radius: 8
            color: Theme.gaugeHub
            border.color: Theme.gaugeHubRing
            border.width: 1.5
        }

        // The glare on the glass.
        Rectangle {
            x: 22
            y: 28
            width: 140
            height: 96
            radius: 48
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.gaugeGlare }
                GradientStop { position: 1; color: "transparent" }
            }
        }
    }

    Text {
        id: labelText

        anchors.horizontalCenter: parent.horizontalCenter
        y: Theme.gaugeSize + Theme.gap2
        text: gauge.label
        color: Theme.gaugeLabel
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeGaugeLabel
        font.weight: Font.DemiBold
        font.letterSpacing: Theme.lampPlacardSpacing
    }
}
