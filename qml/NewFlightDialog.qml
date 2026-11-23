// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// New Flight (design.md §13.1): destination, flight number, the kind of flight, the loads and the money before
// committing (design rule 1). Plan flight fetches the ATIS runways, writes the planned flight and opens SimBrief.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: dialog

    required property NewFlightModel flight

    signal flightPlanned

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(600, parent ? parent.width - 40 : 600)
    height: Math.min(implicitHeight, parent ? parent.height - 40 : implicitHeight)
    padding: Theme.pagePadding

    onOpened: {
        destinationField.text = "";
        numberField.text = dialog.flight.flightNumber;
        destinationField.forceActiveFocus();
    }

    Connections {
        target: dialog.flight

        function onPlanned() {
            dialog.close();
            dialog.flightPlanned();
        }
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: "#99000000"
    }

    contentItem: ScrollView {
        id: scroll

        clip: true
        contentWidth: availableWidth
        implicitHeight: content.implicitHeight

        ColumnLayout {
            id: content

            width: scroll.availableWidth
            spacing: 12

            Text {
                text: "New Flight"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            Text {
                text: dialog.flight.airplaneText + "  ·  from " + dialog.flight.origin
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 4

                Text {
                    text: "DESTINATION"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.labelLetterSpacing
                }

                Text {
                    text: "FLIGHT NUMBER"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.labelLetterSpacing
                }

                TextInputField {
                    id: destinationField

                    Layout.fillWidth: true
                    placeholderText: "ICAO, e.g. KSAN"
                    maximumLength: 4
                    onTextEdited: dialog.flight.destination = text
                }

                TextInputField {
                    id: numberField

                    Layout.preferredWidth: 160
                    placeholderText: "1-4 digits"
                    maximumLength: 4
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,4}/ }
                    onTextEdited: dialog.flight.flightNumber = text
                }
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.flight.destinationMessage !== ""
                text: dialog.flight.destinationMessage
                color: Theme.warn
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            // The kind of flight: a choice only when there's more than one (the BBJ2: VIP airliner or charter).
            RowLayout {
                visible: dialog.flight.kinds.length > 1
                spacing: 8

                Repeater {
                    model: dialog.flight.kinds

                    Rectangle {
                        id: kindButton

                        required property string modelData
                        required property int index

                        readonly property bool chosen: dialog.flight.kind === modelData

                        implicitWidth: kindText.implicitWidth + 28
                        implicitHeight: 32
                        radius: height / 2
                        color: chosen ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : "transparent"
                        border.color: chosen ? Theme.accent : Theme.border
                        border.width: 1

                        Text {
                            id: kindText

                            anchors.centerIn: parent
                            text: dialog.flight.kindLabels[kindButton.index]
                            color: kindButton.chosen ? Theme.textPrimary : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            font.weight: Font.DemiBold
                        }

                        TapHandler {
                            onTapped: dialog.flight.kind = kindButton.modelData
                        }
                    }
                }
            }

            CheckBox {
                id: belly

                visible: dialog.flight.bellyCargoAvailable
                checked: dialog.flight.bellyCargo
                text: "Carry belly cargo"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                onToggled: dialog.flight.bellyCargo = checked

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
                    model: dialog.flight.loadLines

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

            Card {
                Layout.fillWidth: true
                visible: dialog.flight.totalText !== ""
                title: "Money"

                MoneyLines {
                    Layout.fillWidth: true
                    income: dialog.flight.incomeLines
                    costs: dialog.flight.costLines
                    totalText: dialog.flight.totalText
                    fuelNote: dialog.flight.fuelNote
                }

                Text {
                    text: dialog.flight.endsUpText
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.flight.error !== ""
                text: dialog.flight.error
                color: Theme.bad
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                SecondaryButton {
                    text: "Cancel"
                    onClicked: dialog.close()
                }

                PrimaryButton {
                    text: dialog.flight.busy ? "Getting runways…" : "Plan flight"
                    enabled: dialog.flight.canPlan
                    onClicked: dialog.flight.plan()
                }
            }
        }
    }
}
