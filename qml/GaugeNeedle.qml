// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A steam gauge's needle on its 220-unit face: it glides to each new value (D-179) and is still between them. An
// angle given as a running total (the altimeter's hands) keeps turning forward past 360.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: needle

    property real angle: 0
    property real length: 80
    property real tail: 18
    property real halfWidth: 4    // at the hub
    property real tipHalf: 2.4    // just below the point
    property real weight: 0       // a counterweight's radius at the tail (the altimeter's hundreds hand)
    property bool glide: true

    width: 220
    height: 220
    rotation: needle.angle

    Behavior on rotation {
        enabled: needle.glide

        NumberAnimation {
            duration: Theme.needleGlideMs
            easing.type: Easing.Linear  // one sample to the next at a steady pace (D-205)
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillColor: Theme.gaugeNeedle
            startX: 110 - needle.halfWidth
            startY: 110 + needle.tail

            PathLine { x: 110 - needle.tipHalf; y: 110 - needle.length + 10 }
            PathLine { x: 110; y: 110 - needle.length }
            PathLine { x: 110 + needle.tipHalf; y: 110 - needle.length + 10 }
            PathLine { x: 110 + needle.halfWidth; y: 110 + needle.tail }
            PathLine { x: 110 - needle.halfWidth; y: 110 + needle.tail }
        }
    }

    Rectangle {
        visible: needle.weight > 0
        x: 110 - needle.weight
        y: 110 + needle.tail - needle.weight
        width: 2 * needle.weight
        height: width
        radius: needle.weight
        color: Theme.gaugeNeedle
    }
}
