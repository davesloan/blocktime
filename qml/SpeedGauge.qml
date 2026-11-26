// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The speed gauge (D-196): on the ground below 40 kt, ground speed 0-40 kt with a red arc over check 3's 30 kt taxi
// limit; otherwise indicated airspeed 0-400 kt with the Mach number in a window and an amber bug at VREF on approach.
// The panel flips it when the face changes (D-198).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

SteamGauge {
    id: gauge

    property string face: "gs"   // "gs" or "ias"
    property real speedKt: 0
    property string machText
    property real bugKt: -1
    property bool glide: true

    readonly property bool ground: face === "gs"

    function gsAngle(v) { return -150 + 300 * Math.max(0, Math.min(v, 40)) / 40 }
    function iasAngle(v) { return -150 + 300 * Math.max(0, Math.min(v, 400)) / 400 }

    label: ground ? "GROUND SPEED" : "AIRSPEED"

    GaugeArc {
        visible: gauge.ground
        from: gauge.gsAngle(30)
        to: gauge.gsAngle(40)
    }
    GaugeScale {
        visible: gauge.ground
        ticks: {
            const out = []
            for (let v = 0; v <= 40; ++v) out.push({ deg: gauge.gsAngle(v), len: v % 5 ? 7 : 13, w: v % 5 ? 1.2 : 2.2 })
            return out
        }
        labels: [0, 10, 20, 30, 40].map(v => ({ deg: gauge.gsAngle(v), r: 66, text: String(v), size: 15 }))
    }
    GaugeScale {
        visible: !gauge.ground
        ticks: {
            const out = []
            for (let v = 0; v <= 400; v += 10) out.push({ deg: gauge.iasAngle(v), len: v % 50 ? 7 : 13, w: v % 50 ? 1.2 : 2.2 })
            return out
        }
        labels: [50, 100, 150, 200, 250, 300, 350, 400].map(v => ({ deg: gauge.iasAngle(v), r: 66, text: String(v),
                                                                     size: v % 100 ? 11 : 14 }))
    }
    GaugeWindow {
        caption: true
        x: 70
        y: 72
        width: 80
        height: 12
        size: 9
        text: gauge.ground ? "GROUND KT" : "KNOTS"
    }
    GaugeWindow {
        x: 110 - 24
        y: 110 + 27
        width: 48
        height: 22
        text: gauge.ground ? String(Math.round(gauge.speedKt)) : (gauge.machText === "" ? "—" : gauge.machText)
    }

    // The VREF bug on the airspeed face.
    Shape {
        width: 220
        height: 220
        visible: !gauge.ground && gauge.bugKt >= 0
        rotation: gauge.iasAngle(gauge.bugKt)
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
        angle: gauge.ground ? gauge.gsAngle(gauge.speedKt) : gauge.iasAngle(gauge.speedKt)
        glide: gauge.glide
    }
}
