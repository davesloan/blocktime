// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dashboard's header (docs/dashboard.md, D-151, D-152, D-180, D-182, D-184; mockup docs/images/
// dashboard-header-v2.html): the flight and the airline with the block clock, a flat line between the two airplanes
// coloured by altitude (D-255), the phase pill with its marker, and three status lines, with the shutdown checklist after block
// in. Data from DashboardHeader (the flight) and RecorderBridge (the lines, checklist, pill and marker). A click on the
// pill or the marker collapses it to the pill with the airports beside it and the current line (D-187), remembered in
// %LOCALAPPDATA%\BlockTime\BlockTime.ini.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts
import QtCore

Rectangle {
    id: card

    required property DashboardHeader header
    required property RecorderBridge recorder

    // Collapsed (D-187): the pill with the airports' codes beside it, the marker, and only the current status line.
    property bool collapsed: false
    // View Dispatch Report (D-233): Your Flight's accepted flight, not the replay's.
    property bool reportAvailable: false

    signal reportRequested()
    // Room beside the pill for the codes when collapsed; the marker row takes the same margins, so it stays aligned.
    readonly property real codeSpace: collapsed
                                      ? Math.max(originCode.implicitWidth, destinationCode.implicitWidth) + Theme.gap14
                                      : 0
    readonly property int padTop: collapsed ? Theme.gap14 : Theme.headerPadding
    readonly property int padBottom: collapsed ? Theme.gap10 : Theme.headerPadding

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    border.width: Theme.borderWidth
    implicitHeight: column.implicitHeight + padTop + padBottom

    Settings {
        category: "dashboard"
        property alias headerCollapsed: card.collapsed
    }

    // The punctuality line's colour (D-192): early green, delayed red, on time as the other lines.
    function punctualityColor(kind: string): color {
        return kind === "early" ? Theme.good : kind === "delayed" ? Theme.bad : Theme.textSecondary
    }

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
        anchors.leftMargin: Theme.headerPadding
        anchors.rightMargin: Theme.headerPadding
        anchors.topMargin: card.padTop
        anchors.bottomMargin: card.padBottom
        spacing: 0

        // 1. The airline with the flight and the clock level with it, the airports, and the line between the airplanes.
        Item {
            id: top

            visible: !card.collapsed
            Layout.fillWidth: true
            implicitHeight: Theme.headerTopHeight

            component AirportBlock: ColumnLayout {
                id: block

                property string glyph
                property real glyphRotation: 0
                property string code
                property string name
                property string line
                // Under the line: On time / Early / Delayed on the destination (D-192), in its colour.
                property string subLine
                property color subLineColor: Theme.textSecondary
                property bool alignRight: false
                // The glyph's centre and the half-width of its rotated box: the line keeps the marker clear of it.
                readonly property real glyphCenterX: glyphText.x + glyphText.width / 2
                readonly property real glyphCenterY: glyphText.y + glyphText.height / 2
                readonly property real glyphHalfExtent: (glyphText.width + glyphText.height) * Math.SQRT1_2 / 2

                width: Theme.airportBlockWidth
                spacing: 0

                Text {
                    id: glyphText

                    Layout.alignment: block.alignRight ? Qt.AlignRight : Qt.AlignLeft
                    Layout.preferredHeight: Theme.fontSizeAirportIcon + Theme.gap2
                    verticalAlignment: Text.AlignVCenter
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
                    font.pixelSize: Theme.fontSizeFeed
                }

                Text {
                    Layout.fillWidth: true
                    visible: block.subLine !== ""
                    horizontalAlignment: block.alignRight ? Text.AlignRight : Text.AlignLeft
                    text: block.subLine
                    color: block.subLineColor
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeFeed
                    font.weight: Font.DemiBold
                }
            }

            Text {
                id: airline

                anchors.horizontalCenter: parent.horizontalCenter
                height: Theme.headerAirlineHeight
                verticalAlignment: Text.AlignVCenter
                text: card.header.airlineName
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeHeaderAirline
                font.weight: Font.DemiBold
            }

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: airline.verticalCenter
                text: card.header.airlineIcao + " " + card.header.flightNumber
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeHeaderIcao
                font.weight: Font.Bold
            }

            SecondaryButton {
                anchors.right: clockPill.left
                anchors.rightMargin: Theme.gap10
                anchors.verticalCenter: airline.verticalCenter
                visible: card.reportAvailable
                text: "View Dispatch Report"
                onClicked: card.reportRequested()
            }

            Rectangle {
                id: clockPill

                anchors.right: parent.right
                anchors.verticalCenter: airline.verticalCenter
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

            AirportBlock {
                id: origin

                anchors.left: parent.left
                y: Theme.airportBlockTop
                glyph: ""  // Airplane, nose up for the departure
                glyphRotation: -45
                code: card.header.originIcao
                name: card.header.originName
                line: card.header.originLine
            }

            AirportBlock {
                id: destination

                anchors.right: parent.right
                y: Theme.airportBlockTop
                alignRight: true
                glyph: ""  // Airplane, nose down for the arrival
                glyphRotation: 45
                code: card.header.destinationIcao
                name: card.header.destinationName
                line: card.header.destinationLine
                subLine: card.header.punctualityText
                subLineColor: card.punctualityColor(card.header.punctuality)
            }

            // Below the line, between the airports: the flight, the cities and the distance.
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Theme.routeMidTop

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: card.header.flightText
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: card.header.citiesText
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: Theme.gap4
                    text: card.header.distanceText
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }
            }

            // The route line (D-255): flat between the airplanes, its ends leaving room for the marker beside each one.
            Item {
                id: arc

                readonly property real x0: origin.x + origin.glyphCenterX + origin.glyphHalfExtent + Theme.arcGlyphGap
                                           + Theme.arcMarkerSize / 2
                readonly property real x2: destination.x + destination.glyphCenterX - destination.glyphHalfExtent
                                           - Theme.arcGlyphGap - Theme.arcMarkerSize / 2
                readonly property real yEnds: origin.y + origin.glyphCenterY
                readonly property real length: Math.max(0, x2 - x0)
                readonly property real progress: card.header.progress
                readonly property var shares: card.header.arcShares
                readonly property real todShare: card.header.todShare
                // The 1% pieces flown so far, painted ahead of the glide and clipped to it.
                readonly property int pieces: Math.min(shares.length, Math.ceil(progress * shares.length))
                // The share shown: it glides to each sample over its one second, like the gauge needles (D-205). Only
                // the clip and the marker move, so no Canvas repaints on every frame.
                property real shown: progress
                property bool ready: false  // no glide in from 0 when the page first appears

                anchors.fill: parent
                Component.onCompleted: Qt.callLater(() => arc.ready = true)

                Behavior on shown {
                    enabled: arc.ready
                    NumberAnimation {
                        duration: Theme.needleGlideMs
                        easing.type: Easing.Linear
                    }
                }

                // Still to fly: dots at fixed places along the whole line, which the flown line covers as it fills in.
                Canvas {
                    id: dots

                    x: arc.x0 - Theme.arcDotSize
                    y: arc.yEnds - Theme.arcDotSize
                    width: arc.length + 2 * Theme.arcDotSize
                    height: 2 * Theme.arcDotSize
                    onWidthChanged: requestPaint()

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = Theme.textMuted
                        const n = Math.max(1, Math.round(arc.length / Theme.arcDotSpacing))
                        for (let i = 0; i <= n; ++i) {
                            ctx.beginPath()
                            ctx.arc(Theme.arcDotSize + i * arc.length / n, Theme.arcDotSize, Theme.arcDotSize / 2,
                                    0, 2 * Math.PI)
                            ctx.fill()
                        }
                    }
                }

                // Flown: pieces coloured by the highest altitude in each 1% (a gap keeps the last colour), shown up to
                // the gliding share.
                Item {
                    x: arc.x0 - Theme.arcWidth / 2
                    y: arc.yEnds - Theme.arcWidth / 2
                    width: arc.shown > 0 ? Theme.arcWidth / 2 + arc.shown * arc.length : 0
                    height: Theme.arcWidth
                    clip: true

                    Canvas {
                        id: flown

                        readonly property int pieces: arc.pieces
                        readonly property var shares: arc.shares

                        width: arc.length + Theme.arcWidth
                        height: Theme.arcWidth
                        onPiecesChanged: requestPaint()
                        onSharesChanged: requestPaint()
                        onWidthChanged: requestPaint()

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.lineWidth = Theme.arcWidth
                            ctx.lineCap = "round"
                            const n = flown.shares.length
                            const half = Theme.arcWidth / 2
                            let share = 0
                            for (let i = 0; i < flown.pieces; ++i) {
                                if (flown.shares[i] >= 0) share = flown.shares[i]
                                ctx.strokeStyle = card.arcColor(share)
                                ctx.beginPath()
                                ctx.moveTo(half + i / n * arc.length, half)
                                ctx.lineTo(half + (i + 1) / n * arc.length, half)
                                ctx.stroke()
                            }
                        }
                    }
                }

                // The plan's top of descent: a short tick across the line (D-192).
                Rectangle {
                    visible: arc.todShare >= 0
                    x: arc.x0 + arc.todShare * arc.length - width / 2
                    y: arc.yEnds - height / 2
                    width: Theme.arcTodTickWidth
                    height: Theme.arcTodTickLength
                    color: Theme.arcTodTick
                }
            }

            // The airplane on the line, once it's flying; it covers the clipped end of the flown line.
            Rectangle {
                visible: card.header.progress > 0
                x: arc.x0 + arc.shown * arc.length - width / 2
                y: arc.yEnds - height / 2
                width: Theme.arcMarkerSize
                height: Theme.arcMarkerSize
                radius: width / 2
                color: Theme.surface
                border.color: Theme.accent
                border.width: Theme.arcMarkerBorder
            }
        }

        // 2. The phase pill: six equal sections, done cyan, current green.
        Row {
            id: pill

            Layout.fillWidth: true
            Layout.topMargin: Theme.gap4
            Layout.leftMargin: card.codeSpace
            Layout.rightMargin: card.codeSpace
            Layout.preferredHeight: Theme.phasePillHeight
            spacing: Theme.phasePillGap

            readonly property real sectionWidth: (width - Theme.phasePillGap * 5) / 6

            Repeater {
                id: sections

                // A fixed count, so the sections stay and their colours fade when a stage changes (a new list each time
                // would rebuild them in the new colour).
                model: card.recorder.pill.length

                Item {
                    id: section

                    required property int index
                    readonly property var modelData: card.recorder.pill[index]

                    readonly property bool first: index === 0
                    readonly property bool last: index === sections.count - 1
                    readonly property string state_: section.modelData.state
                    readonly property color fill: state_ === "done" ? Theme.phaseDone
                                                : state_ === "current" ? Theme.good : Theme.surfaceRaised

                    width: pill.sectionWidth
                    height: pill.height

                    // The section's shape: rounded at the bar's two ends only.
                    Rectangle {
                        anchors.fill: parent
                        radius: section.first || section.last ? height / 2 : 0
                        color: section.fill

                        Behavior on color { ColorAnimation { duration: Theme.pillColorMs } }
                    }

                    Rectangle {
                        visible: section.first || section.last
                        x: section.first ? parent.width / 2 : 0
                        width: parent.width / 2
                        height: parent.height
                        color: section.fill

                        Behavior on color { ColorAnimation { duration: Theme.pillColorMs } }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: section.modelData.name
                        color: section.state_ === "done" ? Theme.textOnDone
                             : section.state_ === "current" ? Theme.textOnGood : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeFeed
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        // 3. The marker: its point in the pill, sliding to the current sub-phase with a little bounce, the name under it.
        Item {
            id: markerRow

            readonly property var at: card.recorder.pillMarker
            property bool ready: false  // no slide when the page first appears

            Layout.fillWidth: true
            Layout.topMargin: -Theme.pillMarkerOverlap
            Layout.leftMargin: card.codeSpace
            Layout.rightMargin: card.codeSpace
            implicitHeight: Theme.pillMarkerRowHeight
            Component.onCompleted: Qt.callLater(() => markerRow.ready = true)

            Item {
                id: marker

                // In sections (2.5: the middle of the third), so a window resize moves it without a slide.
                property real position: markerRow.at.section + (markerRow.at.part + 0.5) / markerRow.at.parts

                x: position * pill.sectionWidth + Math.min(Math.floor(position), 5) * Theme.phasePillGap
                height: parent.height

                Behavior on position {
                    enabled: markerRow.ready
                    NumberAnimation {
                        duration: Theme.pillMarkerMoveMs
                        easing.type: Easing.OutBack
                        easing.overshoot: Theme.bounceOvershoot
                    }
                }

                Canvas {
                    id: markerShape

                    // A square with a pointed top, with room around it for the shadow.
                    x: -width / 2
                    width: Theme.pillMarkerWidth + Theme.gap4
                    height: Theme.pillMarkerHeight + Theme.gap4

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        const w = Theme.pillMarkerWidth - 1
                        const h = Theme.pillMarkerHeight - 1
                        const left = (width - w) / 2
                        const topY = 0.5
                        const shoulder = topY + h * Theme.pillMarkerShoulder
                        const outline = () => {
                            ctx.beginPath()
                            ctx.moveTo(left + w / 2, topY)
                            ctx.lineTo(left + w, shoulder)
                            ctx.lineTo(left + w, topY + h)
                            ctx.lineTo(left, topY + h)
                            ctx.lineTo(left, shoulder)
                            ctx.closePath()
                        }
                        const fill = ctx.createLinearGradient(0, topY, 0, topY + h)
                        fill.addColorStop(0, Theme.pillMarkerTop)
                        fill.addColorStop(0.45, Theme.bad)
                        fill.addColorStop(1, Theme.pillMarkerBottom)
                        ctx.shadowColor = Theme.pillMarkerShadow
                        ctx.shadowOffsetY = Theme.pillMarkerShadowOffset
                        ctx.shadowBlur = Theme.pillMarkerShadowBlur
                        outline()
                        ctx.fillStyle = fill
                        ctx.fill()
                        ctx.shadowColor = "transparent"
                        ctx.lineWidth = 1
                        ctx.lineJoin = "round"
                        ctx.strokeStyle = Theme.pillMarkerEdge
                        outline()
                        ctx.stroke()
                        // The shine along the point.
                        ctx.globalAlpha = Theme.pillMarkerShineOpacity
                        ctx.beginPath()
                        ctx.moveTo(left + 1.5, shoulder + 0.7)
                        ctx.lineTo(left + w / 2, topY + 2.1)
                        ctx.lineTo(left + w - 1.5, shoulder + 0.7)
                        ctx.stroke()
                        ctx.globalAlpha = 1
                    }
                }

                Text {
                    x: -width / 2
                    y: Theme.pillMarkerHeight + Theme.gap2
                    text: markerRow.at.label
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        // 4. The status lines: now, the one before (with its time) and the one before that. A new line rolls in from
        // above while the others drop a slot, smaller and greyer, and the oldest fades out.
        Item {
            id: status

            readonly property var source: card.recorder.statusLines
            property bool instant: true  // a rebuild (the first fill, a replay seek) doesn't animate

            // A new line pushed on top, as the recorder adds them, rolls; any other change is set without animation.
            function sync(): void {
                const next = status.source
                let topIndex = -1
                let visible = 0
                for (let i = 0; i < lines.count; ++i) {
                    const place = lines.get(i).place
                    if (place === 0) topIndex = i
                    if (place < 3) ++visible
                }
                const topWords = topIndex >= 0 ? lines.get(topIndex).words : ""
                if (topIndex >= 0 && next.length >= 2 && next[0].text !== topWords && next[1].text === topWords) {
                    for (let i = 0; i < lines.count; ++i) {
                        const place = lines.get(i).place + 1
                        lines.setProperty(i, "place", Math.min(place, 3))
                        if (place < next.length && place < 3) lines.setProperty(i, "time", next[place].time)
                    }
                    lines.append({ words: next[0].text, time: next[0].time, place: 0, entering: true })
                    leaving.restart()
                    return
                }
                if (visible === Math.min(next.length, 3)) {
                    for (let i = 0; i < lines.count; ++i) {
                        const place = lines.get(i).place
                        if (place >= 3) continue
                        lines.setProperty(i, "words", next[place].text)
                        lines.setProperty(i, "time", next[place].time)
                    }
                    return
                }
                status.instant = true
                lines.clear()
                for (let i = 0; i < next.length && i < 3; ++i)
                    lines.append({ words: next[i].text, time: next[i].time, place: i, entering: false })
                Qt.callLater(() => status.instant = false)
            }

            Layout.fillWidth: true
            Layout.topMargin: card.collapsed ? 0 : Theme.gap6
            // Collapsed, only the current line's slot shows.
            implicitHeight: card.collapsed ? Theme.statusCollapsedHeight : Theme.statusBlockHeight
            clip: card.collapsed
            onSourceChanged: Qt.callLater(status.sync)  // a burst (a replay seek) becomes one rebuild
            Component.onCompleted: sync()

            ListModel {
                id: lines
            }

            Timer {
                id: leaving

                interval: Theme.statusRemoveMs
                onTriggered: {
                    for (let i = lines.count - 1; i >= 0; --i)
                        if (lines.get(i).place >= 3) lines.remove(i)
                }
            }

            Repeater {
                model: lines

                Text {
                    id: line

                    required property string words
                    required property string time
                    required property int place
                    required property bool entering

                    property bool arrived: !entering
                    readonly property int at: arrived ? Math.min(place, 3) : -1

                    width: status.width
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: line.place === 0 || line.time === "" ? line.words : line.words + " · " + line.time
                    y: at < 0 ? Theme.statusEnterY : at === 0 ? 0 : at === 1 ? Theme.statusSlot1Y
                     : at === 2 ? Theme.statusSlot2Y : Theme.statusLeaveY
                    color: at <= 0 ? Theme.textPrimary : at === 1 ? Theme.textSecondary : Theme.textMuted
                    opacity: at < 0 || at >= 3 ? 0 : 1
                    font.family: Theme.fontFamily
                    font.pixelSize: at <= 0 ? Theme.fontSizeStatus : at === 1 ? Theme.fontSizeFeed
                                  : at === 2 ? Theme.fontSizeSmall : Theme.fontSizeStatusLeaving
                    font.weight: at <= 0 ? Font.DemiBold : Font.Normal
                    Component.onCompleted: if (entering) Qt.callLater(() => { if (line) line.arrived = true })

                    transform: Rotation {
                        origin.x: line.width / 2
                        origin.y: 0
                        axis { x: 1; y: 0; z: 0 }
                        angle: line.at < 0 ? Theme.statusEnterAngle : 0

                        Behavior on angle {
                            enabled: !status.instant
                            NumberAnimation {
                                duration: Theme.statusRollMs
                                easing.type: Easing.OutBack
                                easing.overshoot: Theme.bounceOvershoot
                            }
                        }
                    }

                    Behavior on y {
                        enabled: !status.instant
                        NumberAnimation {
                            duration: Theme.statusRollMs
                            easing.type: Easing.OutBack
                            easing.overshoot: Theme.bounceOvershoot
                        }
                    }
                    Behavior on font.pixelSize {
                        enabled: !status.instant
                        NumberAnimation { duration: Theme.statusFadeMs }
                    }
                    Behavior on color {
                        enabled: !status.instant
                        ColorAnimation { duration: Theme.statusFadeMs }
                    }
                    Behavior on opacity {
                        enabled: !status.instant
                        NumberAnimation { duration: Theme.statusFadeMs }
                    }
                }
            }
        }

        // After block in: the shutdown checklist (D-135), informational only.
        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.gap6
            visible: !card.collapsed && card.recorder.checklist.length > 0
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

    // Collapsed: the airports' codes level with the pill, at the card's edges.
    component PillCode: Text {
        y: column.y + pill.y + (pill.height - height) / 2
        visible: card.collapsed
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeCollapsedCode
        font.weight: Font.Bold
    }
    PillCode {
        id: originCode

        x: column.x
        text: card.header.originIcao
    }
    PillCode {
        id: destinationCode

        x: column.x + column.width - width
        text: card.header.destinationIcao
    }
    // Collapsed: On time / Early / Delayed small under the destination's code (D-192).
    Text {
        x: column.x + column.width - width
        y: destinationCode.y + destinationCode.height
        visible: card.collapsed && card.header.punctualityText !== ""
        text: card.header.punctualityText
        color: card.punctualityColor(card.header.punctuality)
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        font.weight: Font.DemiBold
    }

    // A click anywhere on the pill or its marker collapses or expands the header (D-187).
    MouseArea {
        x: column.x + pill.x
        y: column.y + pill.y
        width: pill.width
        height: markerRow.y + markerRow.height - pill.y
        cursorShape: Qt.PointingHandCursor
        onClicked: card.collapsed = !card.collapsed
    }
}
