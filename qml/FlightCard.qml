// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One flight in the Flights list. Its required properties are the FlightListModel roles, so the page also reads the
// selected flight's numbers from the current card.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

AbstractButton {
    id: card

    required property int index
    required property string dateText
    required property string outText
    required property string inText
    required property string atcId
    required property string title
    required property string blockText
    required property real fuelUsedLb
    required property real distanceNm
    required property real maxAltFt
    required property string status
    required property string statusText

    property bool selected

    readonly property color tone: status === "block_in" ? Theme.good : status === "open" ? Theme.accent : Theme.warn

    implicitHeight: column.implicitHeight + 24
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radiusSmall
        color: card.selected ? Theme.surfaceRaised : card.hovered ? Theme.surfaceHover : Theme.surface
        border.width: 1
        border.color: card.selected ? Theme.accent : Theme.border
    }

    contentItem: ColumnLayout {
        id: column

        spacing: 6

        // Date and time, status
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 12

            Text {
                Layout.fillWidth: true
                text: card.dateText + " · " + card.outText + (card.inText !== "" ? " – " + card.inText : "")
                color: Theme.textSecondary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                font.features: { "tnum": 1 }
            }

            Rectangle {
                Layout.preferredHeight: 20
                Layout.preferredWidth: statusLabel.implicitWidth + 14
                radius: 10
                color: Qt.rgba(card.tone.r, card.tone.g, card.tone.b, 0.12)
                border.color: Qt.rgba(card.tone.r, card.tone.g, card.tone.b, 0.35)
                border.width: 1

                Text {
                    id: statusLabel

                    anchors.centerIn: parent
                    // The pill shows the short form; the reason is on its own line below.
                    text: card.status === "block_in" ? "Block in" : card.status === "open" ? "In progress" : "Interrupted"
                    color: card.tone
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                }
            }
        }

        // Tail and aircraft
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            spacing: 8

            Text {
                text: card.atcId !== "" ? card.atcId : "—"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: card.title
                color: Theme.textSecondary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        // Numbers, or why it was interrupted
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 12
            text: card.status === "interrupted"
                  ? card.statusText
                  : card.blockText + " block · " + card.distanceNm.toFixed(0) + " nm"
                    + (card.fuelUsedLb >= 0 ? " · " + String(Math.round(card.fuelUsedLb)).replace(/\B(?=(\d{3})+(?!\d))/g, ",") + " lb" : "")
            color: card.status === "interrupted" ? Theme.warn : Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.features: { "tnum": 1 }
        }
    }
}
