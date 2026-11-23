// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One settled flight in the Flights list (D-111): date and times, flight number and route, airplane, block time and
// the money it made or lost. Its required properties are FlightListModel roles.

pragma ComponentBehavior: Bound

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
    required property string flightNumber
    required property string routeText
    required property string registration
    required property string typeCode
    required property string blockText
    required property string totalText
    required property real totalCents

    property bool selected

    readonly property color tone: totalCents >= 0 ? Theme.good : Theme.bad

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

        // Date and times, the money
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 12

            Text {
                Layout.fillWidth: true
                text: card.dateText + " · " + card.outText + " – " + card.inText
                color: Theme.textSecondary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                font.features: { "tnum": 1 }
            }

            Text {
                text: card.totalText
                color: card.tone
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
            }
        }

        // Flight number and route
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            spacing: 8

            Text {
                text: card.flightNumber
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: card.routeText
                color: Theme.textPrimary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }

        // Airplane and block time
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 12
            text: card.registration + " · " + card.typeCode + " · " + card.blockText + " block"
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            font.features: { "tnum": 1 }
        }
    }
}
