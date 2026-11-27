// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Pre-Flight panel (docs/dashboard.md, D-153, D-179, D-187, D-189; mockup docs/images/dashboard-preflight-v1.html):
// everything Dave needs at the gate to set up the FMC and the EFB, in the instrument panel look. The load sheet, the
// departure conditions and the clearance, from PreFlightModel. Shows from planning until liftoff.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

InstrumentPanel {
    id: panel

    required property PreFlightModel model

    // The three groups share the width 1 : 1 : 1.15, as in the mockup.
    readonly property real unit: (width - 2 * Theme.panelPaddingSide - 2 * Theme.panelColumnGap) / 3.15

    // "13,439 lb" → "13,439" with "LB" beside it; "30.09 inHg" → "30.09" with "inHg".
    function number(text) { return text.lastIndexOf(" ") < 0 ? text : text.substring(0, text.lastIndexOf(" ")) }
    function unitOf(text) { return text.lastIndexOf(" ") < 0 ? "" : text.substring(text.lastIndexOf(" ") + 1) }
    function holdShort(label) {
        return ({ "Forward": "FWD", "Aft": "AFT", "Main deck": "MAIN", "Bulk": "BULK" })[label] ?? label.toUpperCase()
    }

    component Group: Column {
        id: group

        property string title

        spacing: Theme.readoutGap

        Rectangle {
            width: groupTitle.implicitWidth + 2 * Theme.gap8
            height: groupTitle.implicitHeight + Theme.gap6
            radius: Theme.gap2
            color: Theme.panelTitleBg

            Text {
                id: groupTitle

                anchors.centerIn: parent
                text: group.title
                color: Theme.panelTitleText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizePanelTitle
                font.weight: Font.Bold
                font.letterSpacing: Theme.panelTitleSpacing
            }
        }
    }

    // Two readouts side by side in a group.
    component Pair: Row {
        spacing: Theme.readoutGap
    }

    Row {
        spacing: Theme.panelColumnGap

        Group {
            id: loadSheet

            width: panel.unit
            title: "LOAD SHEET"

            Readout {
                width: loadSheet.width
                title: "Block fuel"
                value: panel.number(panel.model.blockFuelText)
                unit: "LB"
            }
            Readout {
                width: loadSheet.width
                title: "Zero fuel weight"
                value: panel.number(panel.model.zfwText)
                unit: "LB"
            }
            Flow {
                width: loadSheet.width
                spacing: Theme.readoutGap

                Repeater {
                    model: panel.model.passengers

                    Readout {
                        required property var modelData

                        width: panel.model.passengers.length > 1 ? (loadSheet.width - Theme.readoutGap) / 2 : loadSheet.width
                        title: modelData.label
                        value: modelData.value
                    }
                }
            }
            Readout {
                width: loadSheet.width
                title: "Cargo"
                value: panel.number(panel.model.cargoText)
                unit: "LB"

                Row {
                    spacing: Theme.gap14
                    visible: panel.model.holds.length > 0

                    Repeater {
                        model: panel.model.holds

                        Text {
                            required property var modelData

                            text: panel.holdShort(modelData.label) + " " + panel.number(modelData.value)
                            color: Theme.amberDim
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }
            Pair {
                Readout {
                    width: (loadSheet.width - Theme.readoutGap) / 2
                    title: "Cruise"
                    value: panel.model.cruiseText
                }
                Readout {
                    width: (loadSheet.width - Theme.readoutGap) / 2
                    title: "Trans alt"
                    value: panel.model.transitionText
                }
            }
            Text {
                visible: !panel.model.hasPlan
                text: qsTr("Waiting for the SimBrief plan")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        Group {
            id: departure

            width: panel.unit
            title: "DEPARTURE"

            // Where to load in first: the spot it last parked at (D-148, 0.15.1).
            Pair {
                Readout {
                    width: (departure.width - Theme.readoutGap) / 2
                    title: "Gate"
                    value: panel.model.gate
                }
                Readout {
                    width: (departure.width - Theme.readoutGap) / 2
                    title: "ATIS"
                    value: panel.model.atisLetter
                }
            }
            Pair {
                Readout {
                    width: (departure.width - Theme.readoutGap) / 2
                    title: "Runway"
                    value: panel.model.runway
                    tag: panel.model.runwayTag
                    flagged: panel.model.runwayChanged
                }
                Readout {
                    width: (departure.width - Theme.readoutGap) / 2
                    title: "Heading"
                    value: panel.model.headingText
                }
            }
            Pair {
                Readout {
                    width: (departure.width - Theme.readoutGap) / 2
                    title: "Condition"
                    value: panel.model.conditionText
                }
                Readout {
                    width: (departure.width - Theme.readoutGap) / 2
                    title: "OAT"
                    value: panel.model.temperatureText
                }
            }
            Readout {
                width: departure.width
                title: "Wind"
                value: panel.model.windText
            }
            Readout {
                width: departure.width
                title: "Altimeter"
                value: panel.number(panel.model.altimeterText)
                unit: panel.unitOf(panel.model.altimeterText)
            }
        }

        Group {
            id: clearance

            width: panel.unit * 1.15
            title: "CLEARANCE"

            Readout {
                width: clearance.width
                title: "SID"
                value: panel.model.sid
                words: true
                tag: panel.model.sidTag
                flagged: panel.model.sidChanged
            }
            Readout {
                width: clearance.width
                title: "Initial altitude"
                value: panel.model.initialAltitude
                words: true
            }
            Pair {
                // The squawk as a transponder: four digits behind their own windows.
                Rectangle {
                    width: (clearance.width - Theme.readoutGap) / 2
                    height: squawkColumn.implicitHeight + 2 * Theme.gap8
                    radius: Theme.readoutRadius
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.readoutTop }
                        GradientStop { position: 1; color: Theme.readoutBottom }
                    }

                    Column {
                        id: squawkColumn

                        x: Theme.gap10
                        y: Theme.gap8
                        spacing: Theme.gap4

                        Text {
                            text: "SQUAWK"
                            color: Theme.readoutPlacard
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeReadoutTitle
                            font.weight: Font.DemiBold
                            font.letterSpacing: Theme.readoutTitleSpacing
                        }
                        Row {
                            spacing: Theme.gap4

                            Repeater {
                                model: 4

                                Rectangle {
                                    id: digitWindow

                                    required property int index
                                    readonly property string digit: panel.model.squawk.charAt(index)

                                    width: Theme.squawkDigitWidth
                                    height: Theme.squawkDigitHeight
                                    radius: Theme.readoutWindowRadius
                                    color: Theme.readoutWindow

                                    Text {
                                        anchors.centerIn: parent
                                        text: digitWindow.digit === "" ? "—" : digitWindow.digit
                                        color: digitWindow.digit === "" ? Theme.amberBlank : Theme.amber
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSquawk
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Readout {
                width: clearance.width
                visible: panel.model.countdownVisible
                title: "Clearance expires"
                value: panel.model.countdownText
                urgent: panel.model.countdownUrgent
                tag: panel.model.countdownUrgent ? qsTr("Call for a new one") : ""
            }
        }
    }
}
