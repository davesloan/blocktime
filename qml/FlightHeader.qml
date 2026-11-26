// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dashboard's header (docs/dashboard.md, D-151, D-152, D-180, D-182): the identity row, a thin line, the route arc
// coloured by altitude, the phase pill and three status lines, with the shutdown checklist after block in. Data from
// DashboardHeader (the flight) and RecorderBridge (the lines, checklist and pill).

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Rectangle {
    id: card

    required property DashboardHeader header
    required property RecorderBridge recorder

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    border.width: Theme.borderWidth
    implicitHeight: column.implicitHeight + 2 * Theme.headerPadding

    // A share of the plan's cruise altitude (0..1) as the arc's colour, fading between Theme's four stops.
    function arcColor(share: real): color {
        const stops = [Theme.arcGround, Theme.arcLow, Theme.arcHigh, Theme.arcCruise]
        const x = Math.max(0, Math.min(1, share)) * 3
        const i = Math.min(2, Math.floor(x))
        const f = x - i
        const a = stops[i]
        const b = stops[i + 1]
        return Qt.rgba(a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f, 1)
    }

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: Theme.headerPadding
        spacing: Theme.gap12

        // 1. Identity row: ICAO code and flight number | airline, flight, cities | block clock and times.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            ColumnLayout {
                Layout.preferredWidth: column.width / 4
                Layout.alignment: Qt.AlignTop
                spacing: 0

                Text {
                    text: card.header.airlineIcao
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeHeaderIcao
                    font.weight: Font.Bold
                }

                Text {
                    text: card.header.flightNumber
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeHeaderFlight
                    font.weight: Font.DemiBold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.gap2

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: card.header.airlineName
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeHeaderAirline
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: card.header.flightText
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: card.header.citiesText
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }
            }

            ColumnLayout {
                Layout.preferredWidth: column.width / 4
                Layout.alignment: Qt.AlignTop
                spacing: Theme.gap4

                Rectangle {
                    Layout.alignment: Qt.AlignRight
                    implicitWidth: clock.implicitWidth + 2 * Theme.gap14
                    implicitHeight: Theme.headerClockPillHeight
                    radius: height / 2
                    color: "transparent"
                    border.color: card.header.clockIsPlan ? Theme.textMuted : Theme.textSecondary
                    border.width: Theme.borderWidth

                    Text {
                        id: clock

                        anchors.centerIn: parent
                        text: card.header.clockIsPlan ? "Plan " + card.header.clockText : card.header.clockText
                        color: card.header.clockIsPlan ? Theme.textSecondary : Theme.textPrimary
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.fontSizeHeaderClock
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignRight
                    text: "Block out " + card.header.blockOutText
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeFeed
                }

                Text {
                    Layout.alignment: Qt.AlignRight
                    text: "Block in " + card.header.blockInText
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeFeed
                }
            }
        }

        // 2. The thin line: who the flight is, above; the flight happening, below.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hairline
            color: Theme.border
        }

        // 3. The route arc between the two airports.
        Item {
            id: arcRow

            Layout.fillWidth: true
            implicitHeight: Theme.arcRowHeight

            component AirportBlock: ColumnLayout {
                id: block

                property string glyph
                property real glyphRotation: 0
                property string code
                property string name
                property string line
                property bool alignRight: false

                width: Theme.airportBlockWidth
                spacing: Theme.gap2

                Text {
                    Layout.alignment: block.alignRight ? Qt.AlignRight : Qt.AlignLeft
                    text: block.glyph
                    rotation: block.glyphRotation
                    color: Theme.textSecondary
                    font.family: Theme.iconFontFamily
                    font.pixelSize: Theme.fontSizeAirportIcon
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: block.alignRight ? Text.AlignRight : Text.AlignLeft
                    text: block.code
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeAirportCode
                    font.weight: Font.Bold
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: block.alignRight ? Text.AlignRight : Text.AlignLeft
                    text: block.name
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: block.alignRight ? Text.AlignRight : Text.AlignLeft
                    text: block.line
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            AirportBlock {
                id: origin

                anchors.left: parent.left
                anchors.bottom: parent.bottom
                glyph: ""  // Airplane, nose up for the departure
                glyphRotation: -45
                code: card.header.originIcao
                name: card.header.originName
                line: card.header.originLine
            }

            AirportBlock {
                id: destination

                anchors.right: parent.right
                anchors.bottom: parent.bottom
                alignRight: true
                glyph: ""  // Airplane, nose down for the arrival
                glyphRotation: 45
                code: card.header.destinationIcao
                name: card.header.destinationName
                line: card.header.destinationLine
            }

            Canvas {
                id: arc

                // The quadratic curve from the origin's end to the destination's, peaking at the top of the row.
                readonly property real x0: origin.width + Theme.spacing
                readonly property real x2: width - destination.width - Theme.spacing
                readonly property real yEnds: height - Theme.gap12
                readonly property real yControl: -height * 0.55
                readonly property real progress: card.header.progress
                readonly property var shares: card.header.arcShares

                function pointAt(t: real): point {
                    const u = 1 - t
                    const xc = (x0 + x2) / 2
                    return Qt.point(u * u * x0 + 2 * u * t * xc + t * t * x2,
                                    u * u * yEnds + 2 * u * t * yControl + t * t * yEnds)
                }

                anchors.fill: parent
                onProgressChanged: requestPaint()
                onSharesChanged: requestPaint()
                onWidthChanged: requestPaint()

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    // Still to fly: a dotted grey line.
                    ctx.fillStyle = Theme.textMuted
                    for (let t = arc.progress; t <= 1.0001; t += 0.025) {
                        const p = arc.pointAt(t)
                        ctx.beginPath()
                        ctx.arc(p.x, p.y, Theme.arcDotSize / 2, 0, 2 * Math.PI)
                        ctx.fill()
                    }
                    // Flown: short segments coloured by the highest altitude in each 1% (a gap keeps the last colour).
                    ctx.lineWidth = Theme.arcWidth
                    ctx.lineCap = "round"
                    let share = 0
                    const n = arc.shares.length
                    for (let i = 0; i < n && i / n < arc.progress; ++i) {
                        if (arc.shares[i] >= 0) share = arc.shares[i]
                        const a = arc.pointAt(i / n)
                        const b = arc.pointAt(Math.min((i + 1) / n, arc.progress))
                        ctx.strokeStyle = card.arcColor(share)
                        ctx.beginPath()
                        ctx.moveTo(a.x, a.y)
                        ctx.lineTo(b.x, b.y)
                        ctx.stroke()
                    }
                }
            }

            // The airplane on the arc, with its altitude and ground speed above it; shown once it's flying.
            Rectangle {
                id: marker

                readonly property point at: arc.pointAt(card.header.progress)

                visible: card.header.progress > 0
                x: at.x - width / 2
                y: at.y - height / 2
                width: Theme.arcMarkerSize
                height: Theme.arcMarkerSize
                radius: width / 2
                color: Theme.surface
                border.color: Theme.accent
                border.width: Theme.arcMarkerBorder
            }

            Text {
                visible: marker.visible && card.header.progress < 1  // parked: it would crowd the destination
                anchors.horizontalCenter: marker.horizontalCenter
                anchors.bottom: marker.top
                anchors.bottomMargin: Theme.gap6
                text: card.header.markerText
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: arc.yEnds - height - Theme.gap12
                text: card.header.distanceText
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }

        // 4. The phase pill: six equal sections, the recorder's phases as divisions inside the current one.
        Item {
            id: pill

            Layout.fillWidth: true
            implicitHeight: Theme.phasePillHeight

            Row {
                anchors.fill: parent
                spacing: Theme.phasePillGap

                Repeater {
                    id: sections

                    model: card.recorder.pill

                    Item {
                        id: section

                        required property var modelData
                        required property int index

                        readonly property bool first: index === 0
                        readonly property bool last: index === sections.count - 1
                        readonly property color fill: section.colorOf(section.modelData.state)

                        function colorOf(state: string): color {
                            return state === "done" ? Theme.good : state === "current" ? Theme.warn
                                                                                          : Theme.surfaceRaised
                        }

                        width: (pill.width - Theme.phasePillGap * (sections.count - 1)) / Math.max(1, sections.count)
                        height: pill.height

                        // The section's shape: rounded at the bar's two ends only.
                        Rectangle {
                            anchors.fill: parent
                            radius: section.first || section.last ? height / 2 : 0
                            color: section.fill
                        }

                        Rectangle {
                            visible: section.first || section.last
                            x: section.first ? parent.width / 2 : 0
                            width: parent.width / 2
                            height: parent.height
                            color: section.fill
                        }

                        // Divisions: a thin strip along the bottom of the current section only (done green, current
                        // yellow, the rest grey), so every label sits on one solid colour.
                        Row {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: Theme.phaseDivisionHeight
                            visible: section.modelData.state === "current"
                            spacing: Theme.phasePillGap

                            Repeater {
                                model: section.modelData.divisions

                                Rectangle {
                                    id: division

                                    required property string modelData

                                    readonly property int count: section.modelData.divisions.length

                                    width: (section.width - Theme.phasePillGap * (count - 1)) / count
                                    height: Theme.phaseDivisionHeight
                                    color: section.colorOf(division.modelData)
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: section.modelData.name
                            color: section.modelData.state === "done" ? Theme.textOnAccent
                                 : section.modelData.state === "current" ? Theme.textOnWarn : Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeFeed
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }

        // 5. The status lines: now, the one before (with its time) and the one before that.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gap2

            Repeater {
                model: card.recorder.statusLines

                Text {
                    id: statusLine

                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: statusLine.index > 0 && statusLine.modelData.time !== ""
                          ? statusLine.modelData.text + " · " + statusLine.modelData.time : statusLine.modelData.text
                    color: statusLine.index === 0 ? Theme.textPrimary
                         : statusLine.index === 1 ? Theme.textSecondary : Theme.textMuted
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: statusLine.index === 0 ? Theme.fontSizeStatus
                                  : statusLine.index === 1 ? Theme.fontSizeFeed : Theme.fontSizeSmall
                    font.weight: statusLine.index === 0 ? Font.DemiBold : Font.Normal
                }
            }

            // After block in: the shutdown checklist (D-135), informational only.
            Row {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.gap6
                visible: card.recorder.checklist.length > 0
                spacing: Theme.spacing

                Repeater {
                    model: card.recorder.checklist

                    Text {
                        id: check

                        required property var modelData

                        text: (check.modelData.done ? "✓  " : "○  ") + check.modelData.text
                        color: check.modelData.done ? Theme.good : Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }
    }
}
