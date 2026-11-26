// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A radio-stack readout (D-179): an engraved placard over a dark window with amber digits glowing in it. An empty
// value shows a dim dash (not known yet). A tag sits at the window's right: dim amber, or an amber flag for what ATC
// or the ATIS changed. Anything given as children goes under the window (the cargo's holds).

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    id: readout

    property string title
    property string value
    property string unit
    property string tag
    property bool flagged: false  // the tag as an amber flag
    property bool words: false    // a smaller font for words (SID, taxi route)
    property bool urgent: false   // red digits
    default property alias below: extra.data

    readonly property bool blank: value === ""

    radius: Theme.readoutRadius
    implicitHeight: column.implicitHeight + 2 * Theme.gap8
    gradient: Gradient {
        GradientStop { position: 0; color: Theme.readoutTop }
        GradientStop { position: 1; color: Theme.readoutBottom }
    }

    Column {
        id: column

        x: Theme.gap10
        y: Theme.gap8
        width: readout.width - 2 * Theme.gap10
        spacing: Theme.gap4

        Text {
            text: readout.title.toUpperCase()
            color: Theme.readoutPlacard
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeReadoutTitle
            font.weight: Font.DemiBold
            font.letterSpacing: Theme.readoutTitleSpacing
        }

        Rectangle {
            width: column.width
            height: Theme.readoutWindowHeight
            radius: Theme.readoutWindowRadius
            color: Theme.readoutWindow

            Row {
                id: digits

                anchors.left: parent.left
                anchors.leftMargin: Theme.gap8
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 2 * Theme.gap8 - (readout.tag !== "" ? tagBox.width + Theme.gap8 : 0)
                spacing: Theme.gap4
                clip: true

                Text {
                    id: valueText

                    text: readout.blank ? "—" : readout.value
                    color: readout.blank ? Theme.amberBlank : readout.urgent ? Theme.readoutRed : Theme.amber
                    font.family: Theme.monoFontFamily
                    font.pixelSize: readout.words ? Theme.fontSizeReadoutText : Theme.fontSizeReadout
                    font.capitalization: Font.AllUppercase
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, digits.width - (unitText.visible ? unitText.implicitWidth + digits.spacing : 0))
                    // The glow: an outline in a translucent amber. No shader layer, so it draws on every backend
                    // (the offscreen grabs use the software one) and costs nothing per readout.
                    style: readout.blank ? Text.Normal : Text.Outline
                    styleColor: readout.urgent ? Theme.readoutRedGlow : Theme.amberGlow
                }
                Text {
                    id: unitText

                    anchors.baseline: valueText.baseline
                    visible: !readout.blank && readout.unit !== ""
                    text: readout.unit
                    color: Theme.amber
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeReadoutUnit
                }
            }

            Rectangle {
                id: tagBox

                anchors.right: parent.right
                anchors.rightMargin: Theme.gap8
                anchors.verticalCenter: parent.verticalCenter
                visible: readout.tag !== ""
                width: tagText.implicitWidth + (readout.flagged ? 2 * Theme.gap4 : 0)
                height: tagText.implicitHeight
                radius: Theme.gap2
                color: readout.flagged ? Theme.amber : "transparent"

                Text {
                    id: tagText

                    anchors.centerIn: parent
                    visible: readout.tag !== ""
                    text: readout.tag.toUpperCase()
                    color: readout.flagged ? Theme.readoutFlagText : Theme.amberDim
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeReadoutTag
                    font.weight: readout.flagged ? Font.Bold : Font.Normal
                }
            }
        }

        Item {
            id: extra

            width: column.width
            height: childrenRect.height
            visible: children.length > 0
        }
    }
}
