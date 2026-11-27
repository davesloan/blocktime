// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One airplane on the Fleet screen (D-241): its layout first, then its registration, where it is, its current leg,
// and its main button (Fly, Fly Delivery, Continue Flight) beside Manage. Rotation and journey planes get a coloured
// border and ribbon (later).

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Rectangle {
    id: card

    required property string registration
    required property string title
    required property string place
    required property string leg
    required property string glyph
    required property string button
    required property bool buttonEnabled
    required property string group

    signal mainClicked()
    signal manageClicked()

    readonly property color groupColour: card.group === "rotation" ? Theme.rotation : Theme.journey

    implicitWidth: Theme.fleetCardWidth
    implicitHeight: Theme.fleetCardHeight
    radius: Theme.radius
    color: Theme.surfaceRaised
    border.color: card.group !== "" ? card.groupColour : Theme.cardBorder
    border.width: card.group !== "" ? Theme.emptySlotBorderWidth : Theme.borderWidth

    Rectangle {
        visible: card.group !== ""
        anchors.top: parent.top
        anchors.right: parent.right
        width: ribbon.implicitWidth + 2 * Theme.gap10
        height: ribbon.implicitHeight + 2 * Theme.gap4
        radius: Theme.radiusSmall
        color: Theme.surface

        Text {
            id: ribbon

            anchors.centerIn: parent
            text: card.group.toUpperCase()
            color: card.groupColour
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLabel
            font.weight: Font.Bold
            font.letterSpacing: Theme.labelLetterSpacing
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gap14
        spacing: Theme.gap8

        RowLayout {
            spacing: Theme.gap12

            AirplaneGlyph {
                kind: card.glyph
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.gap2

                Text {
                    Layout.fillWidth: true
                    text: card.title
                    elide: Text.ElideRight
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeFleetTitle
                    font.weight: Font.DemiBold
                }

                Text {
                    text: card.registration
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: card.place
            elide: Text.ElideRight
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }

        Rectangle {
            visible: card.leg !== ""
            Layout.fillWidth: true
            implicitHeight: legText.implicitHeight + 2 * Theme.gap6
            radius: Theme.radiusSmall
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth

            Text {
                id: legText

                anchors.fill: parent
                anchors.leftMargin: Theme.gap10
                anchors.rightMargin: Theme.gap10
                verticalAlignment: Text.AlignVCenter
                text: card.leg
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap8

            PrimaryButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 3
                text: card.button
                enabled: card.buttonEnabled
                onClicked: card.mainClicked()
            }

            SecondaryButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                text: "Manage"
                onClicked: card.manageClicked()
            }
        }
    }
}
