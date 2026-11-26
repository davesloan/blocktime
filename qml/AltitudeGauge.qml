// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The altitude gauge (D-202, D-203). The altimeter: three hands on a 0-9 dial read like a clock (a short, wide hand
// for the ten thousands, a long one for the thousands, a skinny one for the hundreds). Below 2,000 ft above the ground
// it becomes the radio altitude, 0-3,000 ft with the bottom 500 ft over half the dial. The setting window is on both.
// The panel flips it when the face changes (D-198).

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

SteamGauge {
    id: gauge

    property string face: "altimeter"  // "altimeter" or "radio"
    property real altitudeFt: 0
    property real aglFt: 0
    property string setting
    property bool glide: true

    readonly property bool radio: face === "radio"

    // The radio altitude's scale: the bottom 500 ft over the first 150°, the rest over the next 150°.
    function raAngle(h) {
        const v = Math.max(0, Math.min(h, 3000))
        return -150 + (v <= 500 ? v / 500 * 150 : 150 + (v - 500) / 2500 * 150)
    }

    label: radio ? "RADIO ALTITUDE" : "ALTITUDE"

    // ---- the altimeter ----
    GaugeScale {
        visible: !gauge.radio
        ticks: {
            const out = []
            for (let i = 0; i < 50; ++i) out.push({ deg: i * 7.2, len: i % 5 ? 7 : 13, w: i % 5 ? 1.2 : 2.2 })
            return out
        }
        labels: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map(i => ({ deg: i * 36, r: 70, text: String(i), size: 17 }))
    }
    GaugeWindow {
        visible: !gauge.radio
        caption: true
        x: 80
        y: 72
        width: 60
        height: 12
        size: 9
        text: "FEET"
    }

    // ---- the radio altitude ----
    GaugeScale {
        visible: gauge.radio
        ticks: {
            const out = []
            for (let h = 0; h <= 500; h += 50) out.push({ deg: gauge.raAngle(h), len: h % 100 ? 7 : 13, w: h % 100 ? 1.2 : 2.2 })
            for (let h = 750; h <= 3000; h += 250) out.push({ deg: gauge.raAngle(h), len: h % 500 ? 7 : 13, w: h % 500 ? 1.2 : 2.2 })
            return out
        }
        labels: [0, 100, 200, 300, 400, 500, 1000, 2000, 3000].map(h => ({ deg: gauge.raAngle(h), r: 68,
                                                                           text: String(h / 100), size: 15 }))
    }
    // "FT × 100" along the bottom of the circle, each letter turned to follow it.
    Repeater {
        model: gauge.radio ? "FT × 100".split("") : []

        Text {
            id: letter

            required property string modelData
            required property int index
            readonly property real deg: 180 + (3.5 - letter.index) * 4.2
            readonly property real rad: letter.deg * Math.PI / 180

            x: 110 + 84 * Math.sin(rad) - width / 2
            y: 110 - 84 * Math.cos(rad) - height / 2
            rotation: letter.deg - 180
            text: letter.modelData
            color: Theme.gaugeCaption
            font.family: Theme.fontFamily
            font.pixelSize: 9
        }
    }

    GaugeWindow {
        x: 110 - 26
        y: 110 + 30
        width: 52
        height: 18
        size: 12
        text: gauge.setting === "" ? "—" : gauge.setting
    }

    // The radio altitude's needle.
    GaugeNeedle {
        visible: gauge.radio
        angle: gauge.raAngle(gauge.aglFt)
        glide: gauge.glide
    }
    // The altimeter's hands, from the whole altitude so a glide across 9 → 0 turns forward.
    GaugeNeedle {  // the ten thousands: short and wide, like an hour hand
        visible: !gauge.radio
        angle: gauge.altitudeFt / 100000 * 360
        length: 46
        tail: 10
        halfWidth: 7
        tipHalf: 6
        glide: gauge.glide
    }
    GaugeNeedle {  // the thousands: long, like a minute hand
        visible: !gauge.radio
        angle: gauge.altitudeFt / 10000 * 360
        length: 72
        tail: 14
        halfWidth: 4
        tipHalf: 3.4
        glide: gauge.glide
    }
    GaugeNeedle {  // the hundreds: long and skinny, with a counterweight, like a second hand
        visible: !gauge.radio
        angle: gauge.altitudeFt / 1000 * 360
        length: 90
        tail: 24
        halfWidth: 1
        tipHalf: 1
        weight: 4.5
        glide: gauge.glide
    }
}
