// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The live flight panel: the live values, the brake and each engine as status tiles (D-140), and the current or last
// flight.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

ColumnLayout {
    id: panel

    required property RecorderBridge recorder

    readonly property bool live: recorder.hasSample
    property double now: Date.now()

    // 12345.6 -> "12,346"
    function grouped(x: real): string {
        return String(Math.round(x)).replace(/\B(?=(\d{3})+(?!\d))/g, ",")
    }

    function signed(x: real): string {
        const r = Math.round(x)
        return (r > 0 ? "+" : "") + grouped(r)
    }

    function twoDigits(n: int): string {
        return n < 10 ? "0" + n : String(n)
    }

    // milliseconds -> "H:MM:SS"
    function clock(ms: real): string {
        const s = Math.max(0, Math.floor(ms / 1000))
        return Math.floor(s / 3600) + ":" + twoDigits(Math.floor(s / 60) % 60) + ":" + twoDigits(s % 60)
    }

    function utcHhmm(d: date): string {
        return d.toISOString().substring(11, 16) + "Z"
    }

    spacing: Theme.spacing

    // Ticks only while a flight is open, once a second, for the block clock. Nothing else redraws on a timer.
    Timer {
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: panel.recorder.flightOpen
        onTriggered: panel.now = Date.now()
    }

    GridLayout {
        id: tiles

        Layout.fillWidth: true
        // As many columns of ~150 px tiles as fit, two to four: the middle of Live's three columns (D-139).
        columns: Math.max(2, Math.min(4, Math.floor((width + columnSpacing) / (Theme.tileWidth + columnSpacing))))
        rowSpacing: Theme.gap12
        columnSpacing: Theme.gap12

        StatTile {
            Layout.fillWidth: true
            label: "Altitude"
            unit: "ft"
            value: panel.live ? panel.grouped(panel.recorder.altitudeFt) : "—"
        }
        // Knots, or Mach from M.70 until it drops below M.66 (D-140).
        StatTile {
            Layout.fillWidth: true
            label: panel.live && panel.recorder.showMach ? "Mach" : "Indicated airspeed"
            unit: panel.live && panel.recorder.showMach ? "" : "kt"
            value: !panel.live ? "—"
                : panel.recorder.showMach ? panel.recorder.machText : panel.grouped(panel.recorder.iasKt)
        }
        StatTile {
            Layout.fillWidth: true
            label: "Ground speed"
            unit: "kt"
            value: panel.live ? panel.grouped(panel.recorder.gsKt) : "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "Vertical speed"
            unit: "fpm"
            value: panel.live ? panel.signed(panel.recorder.vsFpm) : "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "Fuel"
            unit: "lb"
            value: panel.live ? panel.grouped(panel.recorder.fuelLb) : "—"
        }
        StatusTile {
            Layout.fillWidth: true
            label: "Parking brake"
            known: panel.live
            on: panel.recorder.parkingBrake
            onText: "SET"
            offText: "OFF"
        }
        // One tile per engine, 1 on the left (D-140).
        Repeater {
            model: panel.recorder.engineRunning

            StatusTile {
                required property int index
                required property bool modelData

                Layout.fillWidth: true
                label: "Engine " + (index + 1)
                known: panel.live
                on: modelData
                onText: "ON"
                offText: "OFF"
            }
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true
        title: "Flight"

        // A flight is open: block out time, running block clock, fuel at block out.
        ColumnLayout {
            visible: panel.recorder.flightOpen
            Layout.fillWidth: true
            spacing: Theme.gap4

            Text {
                text: "Block out " + (panel.recorder.flightOpen ? panel.utcHhmm(panel.recorder.blockOutTime) : "")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            Text {
                text: panel.recorder.flightOpen ? panel.clock(panel.now - panel.recorder.blockOutTime.getTime()) : ""
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeClock
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
            }

            // The phase with its sub-state, "Climb — level at 9,000 ft" (D-128).
            Text {
                visible: text !== ""
                text: panel.recorder.phase
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            Text {
                text: "Fuel at block out " + panel.grouped(panel.recorder.blockOutFuelLb) + " lb"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }

        // No flight open: the last flight's summary, or how to start one.
        ColumnLayout {
            visible: !panel.recorder.flightOpen
            Layout.fillWidth: true
            spacing: Theme.gap6

            Text {
                text: "No flight open"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: panel.recorder.lastFlightSummary !== ""
                      ? "Last flight: " + panel.recorder.lastFlightSummary
                      : "Release the parking brake at the gate and taxi 10 m to start one."
                color: Theme.textSecondary
                wrapMode: Text.Wrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
