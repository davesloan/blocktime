// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A coloured arc inside a steam gauge's ticks (the taxi limit, low fuel), on its 220-unit face. Angles in degrees
// clockwise from twelve o'clock.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Shape {
    id: arc

    property real from: 0
    property real to: 0
    property color colour: Theme.gaugeRedArc
    property real radius: 89

    width: 220
    height: 220
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        strokeColor: arc.colour
        strokeWidth: 6
        fillColor: "transparent"
        capStyle: ShapePath.FlatCap

        PathAngleArc {
            centerX: 110
            centerY: 110
            radiusX: arc.radius
            radiusY: arc.radius
            startAngle: arc.from - 90
            sweepAngle: arc.to - arc.from
        }
    }
}
