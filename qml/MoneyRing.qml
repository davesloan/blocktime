// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch report's ring (D-234): the fares as the whole circle (the costs for a loss), a slice per cost and a
// green arc for the profit, all worked out in C++ (DispatchModel.ring). Hover lifts a slice outward with a tag just
// outside the ring; click selects it. The lift is a short transition, never a running animation.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: ring

    required property var slices      // [{id, family, shade, label, amountText, start, sweep}]
    required property string selectedId
    required property string centerLabel
    required property string centerValue
    required property string centerSub
    required property string centerTone  // "none", "profit", "loss", "income", "cost"

    property string hoveredId: ""

    signal picked(string id)

    readonly property real centre: width / 2

    function colourOf(family: string, shade: int): color {
        const shades = family === "fuel" ? Theme.ringFuel
                     : family === "airport" ? Theme.ringAirport
                     : family === "crew" ? Theme.ringCrew
                     : family === "profit" ? Theme.ringProfit
                     : Theme.ringService
        return shades[shade % shades.length]
    }

    // The slice under a point, or "" off the ring.
    function sliceAt(px: real, py: real): string {
        const dx = px - ring.centre, dy = py - ring.centre
        const distance = Math.sqrt(dx * dx + dy * dy)
        if (Math.abs(distance - Theme.ringRadius) > Theme.ringStrokeLifted / 2 + Theme.ringLift) return ""
        let turn = Math.atan2(dx, -dy) / (2 * Math.PI)
        if (turn < 0) turn += 1
        for (let i = 0; i < ring.slices.length; ++i) {
            const s = ring.slices[i]
            if (turn >= s.start && turn < s.start + s.sweep) return s.id
        }
        return ""
    }

    function sliceFor(id: string): var {
        for (let i = 0; i < ring.slices.length; ++i) {
            if (ring.slices[i].id === id) return ring.slices[i]
        }
        return null
    }

    implicitWidth: Theme.ringSize
    implicitHeight: Theme.ringSize

    // The empty track.
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.bg
            strokeWidth: Theme.ringStroke
            fillColor: "transparent"

            PathAngleArc {
                centerX: ring.centre
                centerY: ring.centre
                radiusX: Theme.ringRadius
                radiusY: Theme.ringRadius
                startAngle: 0
                sweepAngle: 360
            }
        }
    }

    Repeater {
        model: ring.slices

        Shape {
            id: slice

            required property var modelData

            readonly property bool lifted: ring.hoveredId === modelData.id || ring.selectedId === modelData.id
            readonly property real middle: (modelData.start + modelData.sweep / 2) * 2 * Math.PI
            readonly property real gap: modelData.sweep * 360 > 2 * Theme.ringGapDegrees ? Theme.ringGapDegrees : 0
            property real lift: lifted ? Theme.ringLift : 0

            Behavior on lift {
                NumberAnimation { duration: Theme.ringLiftMs; easing.type: Easing.OutQuad }
            }

            width: ring.width
            height: ring.height
            x: slice.lift * Math.sin(slice.middle)
            y: -slice.lift * Math.cos(slice.middle)
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: ring.colourOf(slice.modelData.family, slice.modelData.shade)
                strokeWidth: slice.lifted ? Theme.ringStrokeLifted : Theme.ringStroke
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap

                PathAngleArc {
                    centerX: ring.centre
                    centerY: ring.centre
                    radiusX: Theme.ringRadius
                    radiusY: Theme.ringRadius
                    startAngle: slice.modelData.start * 360 - 90 + slice.gap / 2
                    sweepAngle: slice.modelData.sweep * 360 - slice.gap
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        width: 2 * (Theme.ringRadius - Theme.ringStroke)
        spacing: Theme.gap2

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: ring.centerLabel
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeFeed
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: ring.centerValue
            color: ring.centerTone === "profit" || ring.centerTone === "income" ? Theme.incomeText
                 : ring.centerTone === "none" ? Theme.textMuted
                 : Theme.costText
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Theme.fontSizeTitle
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeRingValue
            font.weight: Font.ExtraBold
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: ring.centerSub
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: ring.hoveredId !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPositionChanged: mouse => ring.hoveredId = ring.sliceAt(mouse.x, mouse.y)
        onExited: ring.hoveredId = ""
        onClicked: mouse => {
            const id = ring.sliceAt(mouse.x, mouse.y)
            if (id !== "") ring.picked(id)
        }
    }

    // The hover tag: name and amount, just outside the ring at the slice's middle.
    Rectangle {
        id: tag

        readonly property var slice: ring.sliceFor(ring.hoveredId)
        readonly property real middle: slice ? (slice.start + slice.sweep / 2) * 2 * Math.PI : 0

        visible: slice !== null
        x: ring.centre + Theme.ringTagRadius * Math.sin(middle) - width / 2
        y: ring.centre - Theme.ringTagRadius * Math.cos(middle) - height / 2
        z: 1
        width: tagRow.implicitWidth + 2 * Theme.gap10
        height: tagRow.implicitHeight + 2 * Theme.gap6
        radius: Theme.radiusSmall
        color: Theme.bg
        border.color: slice ? ring.colourOf(slice.family, slice.shade) : Theme.border
        border.width: Theme.borderWidth

        Row {
            id: tagRow

            anchors.centerIn: parent
            spacing: Theme.gap6

            Text {
                text: tag.slice ? tag.slice.label : ""
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeFeed
                font.weight: Font.Bold
            }

            Text {
                text: tag.slice ? tag.slice.amountText : ""
                color: tag.slice && tag.slice.family === "profit" ? Theme.incomeText : Theme.costText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeFeed
            }
        }
    }
}
