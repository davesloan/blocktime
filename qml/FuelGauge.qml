// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fuel gauge (D-196, D-203): total fuel from 0 to the block-out fuel rounded up to 6,000 lb, a red arc up to the
// plan's final reserve, and the pounds in a window.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

SteamGauge {
    id: gauge

    property real fuelLb: 0
    property real scaleLb: 6000
    property real lowLb: 0
    property string text
    property bool glide: true
    property real bugLb: -1  // the plan's fuel here (D-192); -1 = no bug

    function fuelAngle(lb) { return -120 + 240 * Math.max(0, Math.min(lb, scaleLb)) / Math.max(scaleLb, 1) }

    label: "FUEL"

    GaugeArc {
        visible: gauge.lowLb > 0
        from: gauge.fuelAngle(0)
        to: gauge.fuelAngle(gauge.lowLb)
    }
    GaugeScale {
        ticks: {
            const out = []
            for (let i = 0; i <= 36; ++i) out.push({ deg: -120 + i * 240 / 36, len: i % 6 ? 7 : 13, w: i % 6 ? 1.2 : 2.2 })
            return out
        }
        labels: [0, 1, 2, 3, 4, 5, 6].map(i => ({ deg: -120 + i * 40, r: 68,
                                                  text: String(Math.round(gauge.scaleLb / 6 * i / 1000)), size: 15 }))
    }
    GaugeWindow { caption: true; x: 70; y: 128; width: 80; height: 12; size: 9; text: "LB × 1000" }
    GaugeWindow {
        x: 110 - 30
        y: 110 + 34
        width: 60
        height: 19
        size: 13
        text: gauge.text === "" ? "—" : gauge.text
    }

    // The plan's fuel for this point: an amber bug, as the airspeed gauge's (D-179).
    Shape {
        width: 220
        height: 220
        visible: gauge.bugLb >= 0
        rotation: gauge.fuelAngle(gauge.bugLb)
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillColor: Theme.gaugeBug
            startX: 104
            startY: 16

            PathLine { x: 116; y: 16 }
            PathLine { x: 116; y: 24 }
            PathLine { x: 110; y: 30 }
            PathLine { x: 104; y: 24 }
            PathLine { x: 104; y: 16 }
        }
    }

    GaugeNeedle {
        angle: gauge.fuelAngle(gauge.fuelLb)
        glide: gauge.glide
    }
}
