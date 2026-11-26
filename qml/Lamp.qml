// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// An indicator lamp on the in-flight panel's strip (D-179, D-195): a domed glass lens in a metal ring with an
// incandescent bulb behind it, an engraved placard under it. Lit means the switch is on; while it's wrong the lamp
// flashes with the model's 1 s timer; without data it stays dark.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: lamp

    property string name
    property string lens: "amber"  // "red", "white", "amber" or "green"
    property bool known: false
    property bool lit: false
    property bool wrong: false
    property bool flashOn: true    // the model's flash phase

    readonly property bool shown: known && (wrong ? flashOn : lit)
    readonly property var colours: Theme.lampLenses[lens]

    implicitWidth: Theme.lampWidth
    implicitHeight: bezel.height + Theme.gap6 + placard.height

    // The glow around a lit lens.
    Shape {
        x: bezel.x - Theme.lampGlow
        y: bezel.y - Theme.lampGlow
        width: bezel.width + 2 * Theme.lampGlow
        height: width
        visible: lamp.shown
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: Theme.lampSize / 2 + Theme.lampGlow
                centerY: centerX
                centerRadius: centerX
                focalX: centerX
                focalY: centerY
                GradientStop { position: 0.4; color: lamp.colours.glow }
                GradientStop { position: 1; color: "transparent" }
            }
            PathAngleArc {
                centerX: Theme.lampSize / 2 + Theme.lampGlow
                centerY: centerX
                radiusX: centerX
                radiusY: centerX
                startAngle: 0
                sweepAngle: 360
            }
        }
    }

    // The metal ring.
    Rectangle {
        id: bezel

        anchors.horizontalCenter: parent.horizontalCenter
        width: Theme.lampSize
        height: Theme.lampSize
        radius: width / 2
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.lampBezelLight }
            GradientStop { position: 0.5; color: Theme.lampBezelDark }
            GradientStop { position: 1; color: Theme.lampBezelLight }
        }

        // The lens: a radial gradient, bright in the middle when lit.
        Shape {
            anchors.fill: parent
            anchors.margins: Theme.gap4
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                id: lensPath

                readonly property real r: (Theme.lampSize - 2 * Theme.gap4) / 2

                strokeWidth: 0
                strokeColor: "transparent"
                fillGradient: RadialGradient {
                    centerX: lensPath.r
                    centerY: lensPath.r * 1.1
                    centerRadius: lensPath.r
                    focalX: centerX
                    focalY: centerY
                    GradientStop { position: 0; color: lamp.shown ? lamp.colours.on[0] : lamp.colours.off[0] }
                    GradientStop { position: 0.2; color: lamp.shown ? lamp.colours.on[1] : lamp.colours.off[0] }
                    GradientStop { position: 0.5; color: lamp.shown ? lamp.colours.on[2] : lamp.colours.off[1] }
                    GradientStop { position: 1; color: lamp.shown ? lamp.colours.on[3] : lamp.colours.off[1] }
                }
                PathAngleArc {
                    centerX: lensPath.r
                    centerY: lensPath.r
                    radiusX: lensPath.r
                    radiusY: lensPath.r
                    startAngle: 0
                    sweepAngle: 360
                }
            }
        }

        // The highlight on the glass dome.
        Rectangle {
            x: parent.width * 0.3
            y: parent.height * 0.22
            width: parent.width * 0.3
            height: parent.height * 0.2
            radius: height / 2
            color: Theme.gaugeGlare
        }
    }

    Rectangle {
        id: placard

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: bezel.bottom
        anchors.topMargin: Theme.gap6
        width: placardText.implicitWidth + 2 * Theme.gap6
        height: placardText.implicitHeight + Theme.gap4
        radius: Theme.gap2
        color: Theme.panelTitleBg

        Text {
            id: placardText

            anchors.centerIn: parent
            text: lamp.name
            color: Theme.lampPlacardText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLampPlacard
            font.weight: Font.DemiBold
            font.letterSpacing: Theme.lampPlacardSpacing
        }
    }
}
