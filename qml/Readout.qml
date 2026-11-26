// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A radio-stack readout (D-179): an engraved placard over a dark window with amber digits glowing in it. An empty
// value shows a dim dash (not known yet). The tag sits on the title row, right-aligned (D-203): dim amber, or an amber
// flag for what ATC or the ATIS changed. LATEST lights the placard amber, like an annunciator. Anything given as
// children goes under the window (the cargo's holds).

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
    property bool latest: false   // the ATC card: the last call changed this slot (D-197)
    property int stars: -1        // the landing's 0-5 stars instead of a value (D-158)
    default property alias below: extra.data

    readonly property bool blank: value === "" && stars < 0

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

        // The title row: the placard on the left, the tag on the right.
        Item {
            width: column.width
            height: Math.max(placard.height, tagBox.height)

            Rectangle {
                id: placard

                x: readout.latest ? -Theme.gap4 : 0
                width: titleText.implicitWidth + (readout.latest ? 2 * Theme.gap4 : 0)
                height: titleText.implicitHeight
                radius: Theme.gap2
                color: readout.latest ? Theme.amber : "transparent"

                Text {
                    id: titleText

                    anchors.centerIn: parent
                    text: readout.title.toUpperCase()
                    color: readout.latest ? Theme.readoutLatestText : Theme.readoutPlacard
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeReadoutTitle
                    font.weight: readout.latest ? Font.Bold : Font.DemiBold
                    font.letterSpacing: Theme.readoutTitleSpacing
                }
            }

            Rectangle {
                id: tagBox

                anchors.right: parent.right
                anchors.verticalCenter: placard.verticalCenter
                visible: readout.tag !== "" && !readout.blank
                width: Math.min(tagText.implicitWidth + (readout.flagged ? 2 * Theme.gap4 : 0),
                                column.width - placard.width - Theme.gap6)
                height: tagText.implicitHeight
                radius: Theme.gap2
                color: readout.flagged ? Theme.amber : "transparent"

                Text {
                    id: tagText

                    anchors.centerIn: parent
                    width: parent.width - (readout.flagged ? 2 * Theme.gap4 : 0)
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideLeft
                    text: readout.tag.toUpperCase()
                    color: readout.flagged ? Theme.readoutFlagText : Theme.amberDim
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeReadoutTag
                    font.weight: readout.flagged ? Font.Bold : Font.Normal
                }
            }
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
                width: parent.width - 2 * Theme.gap8
                spacing: Theme.gap4
                clip: true

                Text {
                    id: valueText

                    visible: readout.stars < 0
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
                    visible: !readout.blank && readout.unit !== "" && readout.stars < 0
                    text: readout.unit
                    color: Theme.amber
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeReadoutUnit
                }
                // The landing's stars: lit ones amber, the rest dim.
                Text {
                    visible: readout.stars >= 0
                    text: "★".repeat(Math.max(0, readout.stars))
                    color: Theme.amber
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeReadout
                    style: Text.Outline
                    styleColor: Theme.amberGlow
                }
                Text {
                    visible: readout.stars >= 0 && readout.stars < 5
                    text: "★".repeat(5 - Math.max(0, readout.stars))
                    color: Theme.amberBlank
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeReadout
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
