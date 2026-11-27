// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// An airplane seen from above, coloured by what Dave can do with it (D-241): ready, awaiting delivery, his current
// flight, or flying a background leg.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: glyph

    property string kind: "ready"  // "ready" | "delivery" | "current" | "air"
    property real size: Theme.fleetGlyphSize

    implicitWidth: size
    implicitHeight: size

    Shape {
        // Drawn on a 64-unit square and scaled to `size`.
        width: 64
        height: 64
        scale: glyph.size / 64
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: -1
            fillColor: glyph.kind === "delivery" ? Theme.glyphDelivery
                     : glyph.kind === "current" ? Theme.glyphCurrent
                     : glyph.kind === "air" ? Theme.glyphAir
                     : Theme.glyphReady

            PathSvg {
                path: "M32 4c2 0 3 3 3 6v14l22 12v5l-22-6v13l7 5v4l-10-3-10 3v-4l7-5V35l-22 6v-5l22-12V10c0-3 1-6 3-6z"
            }
        }
    }
}
