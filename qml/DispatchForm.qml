// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch form (docs/dispatch.md, step 2): the airplane, destination, flight number, kind, fully loaded or random
// loads, belly cargo, extra fuel and the rolled loads, then Dispatch. Opened from Fleet's Fly (or At Gate
// Turnaround) pre-filled; with no airplane chosen it points at Fleet. Change flight reopens it with the flight's loads.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    id: form

    required property DispatchModel model

    signal fleetRequested()

    implicitWidth: Theme.formWidth
    implicitHeight: column.implicitHeight + 2 * Theme.pagePadding
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    border.width: Theme.borderWidth

    component FormLabel: Text {
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeLabel
        font.weight: Font.DemiBold
        font.letterSpacing: Theme.labelLetterSpacing
    }

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap12

        FormLabel {
            text: form.model.changing ? "CHANGE FLIGHT" : "NEW FLIGHT"
        }

        // No airplane yet: flights start from Fleet.
        ColumnLayout {
            Layout.fillWidth: true
            visible: !form.model.formReady
            spacing: Theme.gap12

            Text {
                Layout.fillWidth: true
                text: "No flight planned. Pick an airplane on Fleet and press Fly to set one up here."
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
            }

            PrimaryButton {
                text: "Go to Fleet"
                onClicked: form.fleetRequested()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: form.model.formReady
            spacing: Theme.gap12

            Text {
                text: form.model.airplaneText
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeReportTitle
                font.weight: Font.Bold
            }

            Text {
                text: "From " + form.model.origin
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: Theme.gap12
                rowSpacing: Theme.gap4

                FormLabel { text: "DESTINATION" }
                FormLabel { text: "FLIGHT NUMBER" }
                FormLabel { text: "EXTRA FUEL (LB)" }

                TextInputField {
                    Layout.fillWidth: true
                    placeholderText: "ICAO, e.g. KSAN"
                    maximumLength: 4
                    text: form.model.destination
                    onTextEdited: form.model.destination = text
                }

                TextInputField {
                    Layout.preferredWidth: Theme.shortFieldWidth
                    placeholderText: "1-4 digits"
                    maximumLength: 4
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,4}/ }
                    text: form.model.flightNumber
                    onTextEdited: form.model.flightNumber = text
                }

                TextInputField {
                    Layout.preferredWidth: Theme.shortFieldWidth
                    placeholderText: "none"
                    maximumLength: 6
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,6}/ }
                    text: form.model.additionalFuel
                    onTextEdited: form.model.additionalFuel = text
                }
            }

            Text {
                Layout.fillWidth: true
                visible: form.model.destinationMessage !== ""
                text: form.model.destinationMessage
                color: Theme.warn
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            // The kind (a choice only for the BBJ2: VIP airliner or charter), then fully loaded or random.
            RowLayout {
                spacing: Theme.gap8

                Repeater {
                    model: form.model.kinds.length > 1 ? form.model.kinds : []

                    Chip {
                        required property string modelData
                        required property int index

                        text: form.model.kindLabels[index]
                        chosen: form.model.kind === modelData
                        onTapped: form.model.kind = modelData
                    }
                }

                Item {
                    visible: form.model.kinds.length > 1 && form.model.fullyLoadedAvailable
                    implicitWidth: Theme.spacing
                }

                Chip {
                    visible: form.model.fullyLoadedAvailable
                    text: "Random loads"
                    chosen: !form.model.fullyLoaded
                    onTapped: form.model.fullyLoaded = false
                }

                Chip {
                    visible: form.model.fullyLoadedAvailable
                    text: "Fully loaded"
                    chosen: form.model.fullyLoaded
                    onTapped: form.model.fullyLoaded = true
                }
            }

            CheckBox {
                id: belly

                visible: form.model.bellyCargoAvailable
                checked: form.model.bellyCargo
                text: "Carry belly cargo"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                onToggled: form.model.bellyCargo = checked

                contentItem: Text {
                    leftPadding: belly.indicator.width + belly.spacing
                    text: belly.text
                    font: belly.font
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Card {
                Layout.fillWidth: true
                title: "Load"

                Repeater {
                    model: form.model.loadLines

                    Text {
                        required property string modelData

                        Layout.fillWidth: true
                        text: modelData
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: form.model.message
                color: Theme.warn
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gap8

                Item {
                    Layout.fillWidth: true
                }

                SecondaryButton {
                    text: "Cancel"
                    onClicked: form.model.closeForm()
                }

                PrimaryButton {
                    text: form.model.busy ? "Dispatching…" : form.model.changing ? "Save changes" : "Dispatch"
                    enabled: form.model.canDispatch
                    onClicked: form.model.changing ? form.model.saveChange() : form.model.dispatch()
                }
            }
        }
    }

    component Chip: Rectangle {
        id: chip

        property string text
        property bool chosen

        signal tapped()

        implicitWidth: chipText.implicitWidth + 2 * Theme.gap14
        implicitHeight: Theme.chipHeight
        radius: height / 2
        color: chip.chosen ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : "transparent"
        border.color: chip.chosen ? Theme.accent : Theme.border
        border.width: Theme.borderWidth

        Text {
            id: chipText

            anchors.centerIn: parent
            text: chip.text
            color: chip.chosen ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
        }

        TapHandler {
            onTapped: chip.tapped()
        }
    }
}
