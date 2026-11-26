// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The vertical speed gauge (D-196): 0 at nine o'clock, ±6,000 fpm, the first 1,000 opened up.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

SteamGauge {
    id: gauge

    property real vsFpm: 0
    property bool glide: true

    // Thousands of fpm to degrees from twelve o'clock.
    function vsAngle(v) {
        const a = Math.min(Math.abs(v), 6)
        const d = a <= 1 ? a * 60 : a <= 2 ? 60 + (a - 1) * 40 : 100 + (a - 2) * 17.5
        return -90 + Math.sign(v) * d
    }

    label: "VERTICAL SPEED"

    GaugeScale {
        ticks: {
            const out = []
            for (const v of [-6, -4, -2, -1, 0, 1, 2, 4, 6]) out.push({ deg: gauge.vsAngle(v), len: 14, w: 2.2 })
            for (let v = -5.5; v <= 5.5; v += 1) out.push({ deg: gauge.vsAngle(v), len: 7, w: 1.2 })
            return out
        }
        labels: [-6, -4, -2, -1, 0, 1, 2, 4, 6].map(v => ({ deg: gauge.vsAngle(v), r: 68, text: String(Math.abs(v)),
                                                            size: 15 }))
    }
    GaugeWindow { caption: true; x: 132; y: 86; width: 24; height: 12; size: 10; text: "UP" }
    GaugeWindow { caption: true; x: 132; y: 124; width: 24; height: 12; size: 10; text: "DN" }
    GaugeWindow { caption: true; x: 60; y: 146; width: 100; height: 12; size: 9; text: "FT/MIN × 1000" }

    GaugeNeedle {
        angle: gauge.vsAngle(gauge.vsFpm / 1000)
        glide: gauge.glide
    }
}
