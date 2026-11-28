// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch report's plan card (D-234). Before import: the three steps, Open SimBrief and Import. After: the two
// airports with an altitude-coloured line, runways, SID and STAR, the plan's six figures, the route with Copy route
// (for Navigraph Charts), then Open SimBrief again and Re-import. Re-import goes once the flight blocks out.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Card {
    id: card

    required property DispatchModel model

    title: card.model.planTitle

    // Before import: what to do.
    Repeater {
        model: card.model.imported ? [] : [
            "Open SimBrief: the dispatch page opens filled in with this flight, its loads and your extra fuel.",
            "Check the route and press Generate there.",
            "Import the plan here. The money flips in."
        ]

        RowLayout {
            id: step

            required property string modelData
            required property int index

            Layout.fillWidth: true
            spacing: Theme.gap10

            Text {
                Layout.alignment: Qt.AlignTop
                text: step.index + 1
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.Bold
            }

            Text {
                Layout.fillWidth: true
                text: step.modelData
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }
    }

    // After import: the plan.
    RowLayout {
        Layout.fillWidth: true
        visible: card.model.imported
        spacing: Theme.gap14

        Text {
            text: card.model.originIcao
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizePlanAirport
            font.weight: Font.Bold
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.planLineHeight
            radius: height / 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.arcGround }
                GradientStop { position: 0.12; color: Theme.arcLow }
                GradientStop { position: 0.25; color: Theme.arcHigh }
                GradientStop { position: 0.4; color: Theme.arcCruise }
                GradientStop { position: 0.7; color: Theme.arcCruise }
                GradientStop { position: 0.82; color: Theme.arcHigh }
                GradientStop { position: 0.92; color: Theme.arcLow }
                GradientStop { position: 1.0; color: Theme.arcGround }
            }
        }

        Text {
            text: card.model.destinationIcao
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizePlanAirport
            font.weight: Font.Bold
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: card.model.imported

        Text {
            Layout.fillWidth: true
            text: card.model.departureText
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeFeed
        }

        Text {
            text: card.model.arrivalText
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeFeed
        }
    }

    GridLayout {
        Layout.fillWidth: true
        Layout.topMargin: Theme.gap8
        visible: card.model.imported
        columns: 3
        columnSpacing: Theme.gap12
        rowSpacing: Theme.gap12

        Repeater {
            model: card.model.planCells

            ColumnLayout {
                id: cell

                required property var modelData

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                spacing: Theme.gap4

                Text {
                    text: cell.modelData.label
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.labelLetterSpacing
                }

                Text {
                    text: cell.modelData.value
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizePlanCell
                    font.weight: Font.Bold
                }
            }
        }
    }

    // The route for Navigraph Charts, with Copy route beside it (§13.3).
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Theme.gap8
        visible: card.model.imported
        spacing: Theme.gap8

        Text {
            Layout.fillWidth: true
            text: card.model.routeForCharts
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.fontSizeFeed
        }

        SecondaryButton {
            text: "Copy route"
            onClicked: card.model.copyRoute()
        }
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Theme.gap10
        visible: !card.model.locked
        spacing: Theme.gap10

        SecondaryButton {
            text: card.model.openSimbriefLabel + " ↗"
            enabled: !card.model.busy
            onClicked: card.model.openSimbrief()

            background: Rectangle {
                implicitHeight: Theme.controlHeight
                radius: Theme.radiusSmall
                color: Theme.border
                border.color: Theme.slotEmptyBorder
                border.width: Theme.borderWidth
            }
        }

        PrimaryButton {
            text: card.model.busy ? "Importing…" : card.model.importLabel
            enabled: !card.model.busy
            onClicked: card.model.importPlan()
        }
    }
}
