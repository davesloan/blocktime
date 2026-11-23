// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The live flight panel: aircraft, eight live values, and the current or last flight.

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

    Card {
        Layout.fillWidth: true
        title: "Aircraft"

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: panel.live ? panel.recorder.aircraft : "No aircraft yet"
                color: panel.live ? Theme.textPrimary : Theme.textSecondary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            Text {
                visible: panel.live && panel.recorder.atcId !== ""
                text: panel.recorder.atcId
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
                font.letterSpacing: Theme.labelLetterSpacing
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 4
        rowSpacing: 12
        columnSpacing: 12

        StatTile {
            Layout.fillWidth: true
            label: "Altitude"
            unit: "ft"
            value: panel.live ? panel.grouped(panel.recorder.altitudeFt) : "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "Indicated airspeed"
            unit: "kt"
            value: panel.live ? panel.grouped(panel.recorder.iasKt) : "—"
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
        StatTile {
            Layout.fillWidth: true
            label: "Parking brake"
            value: panel.live ? (panel.recorder.parkingBrake ? "SET" : "OFF") : "—"
            accent: panel.live && panel.recorder.parkingBrake ? Theme.warn : Theme.textPrimary
        }
        StatTile {
            Layout.fillWidth: true
            label: "Position"
            value: panel.live ? (panel.recorder.onGround ? "GROUND" : "AIR") : "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "Engines running"
            value: panel.live ? panel.recorder.enginesRunning + " / " + panel.recorder.engineCount : "—"
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
            spacing: 4

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
            spacing: 6

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
