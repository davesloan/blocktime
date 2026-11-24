// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The planned flight before departure (design.md §13.2-13.3): its state, where to load in, the ATIS runways, the
// route to copy, the plan and the money estimate. Import Active SimBrief Plan fetches the plan once per click (D-101).
// The chevron collapses it to its title line, to give the flight panel room.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Card {
    id: panel

    required property PlannedFlightModel flight

    property bool collapsed: false

    readonly property color stateTone: flight.state === "simConnected" || flight.state === "flying" ? Theme.good
                                     : flight.state === "waitingForPlan" ? Theme.warn
                                     : Theme.textSecondary

    title: "Planned flight"

    component Line: Text {
        Layout.fillWidth: true
        color: Theme.textSecondary
        wrapMode: Text.WordWrap
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.gap12

        Text {
            text: panel.flight.title
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeTitle
            font.weight: Font.DemiBold
        }

        Rectangle {
            implicitWidth: stateText.implicitWidth + 2 * Theme.gap10
            implicitHeight: Theme.badgeHeight + Theme.gap2
            radius: height / 2
            color: Qt.rgba(panel.stateTone.r, panel.stateTone.g, panel.stateTone.b, 0.12)
            border.color: Qt.rgba(panel.stateTone.r, panel.stateTone.g, panel.stateTone.b, 0.35)
            border.width: Theme.borderWidth

            Text {
                id: stateText

                anchors.centerIn: parent
                text: panel.flight.stateText
                color: panel.stateTone
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                font.weight: Font.DemiBold
            }
        }

        Item {
            Layout.fillWidth: true
        }

        // Collapse or expand: a chevron from the system icon font (D-062).
        Rectangle {
            implicitWidth: Theme.chipHeight
            implicitHeight: Theme.chipHeight
            radius: height / 2
            color: chevronHover.hovered ? Theme.surfaceRaised : "transparent"
            border.color: chevronHover.hovered ? Theme.textPrimary : Theme.textSecondary
            border.width: Theme.borderWidth

            Text {
                anchors.centerIn: parent
                text: panel.collapsed ? "\uE70D" : "\uE70E"  // ChevronDown : ChevronUp
                color: Theme.textPrimary
                font.family: Theme.iconFontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            HoverHandler {
                id: chevronHover

                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                onTapped: panel.collapsed = !panel.collapsed
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: !panel.collapsed
        spacing: Theme.spacing * 2

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.gap6

            Line {
                text: panel.flight.airplaneText + "  ·  " + panel.flight.kindText
            }

            Line {
                text: panel.flight.loadInText
                color: Theme.textPrimary
            }

            Line {
                text: panel.flight.runwayText
            }

            // The route, for Navigraph Charts (design.md §13.3).
            RowLayout {
                Layout.fillWidth: true
                visible: panel.flight.route !== ""
                spacing: Theme.gap8

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: routeText.implicitHeight + 16
                    radius: Theme.radiusSmall
                    color: Theme.bg
                    border.color: Theme.border
                    border.width: Theme.borderWidth

                    Text {
                        id: routeText

                        anchors.fill: parent
                        anchors.margins: Theme.gap8
                        text: panel.flight.route
                        color: Theme.textPrimary
                        wrapMode: Text.WrapAnywhere
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }

                SecondaryButton {
                    text: "Copy route"
                    onClicked: panel.flight.copyRoute()
                }
            }

            Line {
                visible: panel.flight.planText !== ""
                text: panel.flight.planText
            }

            Line {
                visible: panel.flight.warning !== ""
                text: panel.flight.warning
                color: Theme.warn
            }

            Line {
                visible: panel.flight.message !== ""
                text: panel.flight.message
                color: Theme.textPrimary
            }

            RowLayout {
                Layout.topMargin: Theme.gap4
                spacing: Theme.gap8

                PrimaryButton {
                    text: panel.flight.busy ? "Importing…" : "Import Active SimBrief Plan"
                    enabled: !panel.flight.busy
                    onClicked: panel.flight.importPlan()
                }

                SecondaryButton {
                    text: "Open SimBrief again"
                    onClicked: panel.flight.openDispatchPage()
                }

                SecondaryButton {
                    visible: panel.flight.state !== "flying"  // cancelling mid-flight would lose the flight
                    text: "Cancel flight"
                    tone: Theme.bad
                    onClicked: confirmCancel.open()
                }
            }
        }

        ColumnLayout {
            Layout.preferredWidth: Theme.liveFeedWidth
            Layout.alignment: Qt.AlignTop
            spacing: Theme.gap6

            Text {
                text: "ESTIMATE"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                font.weight: Font.DemiBold
                font.letterSpacing: Theme.labelLetterSpacing
            }

            MoneyLines {
                Layout.fillWidth: true
                costs: panel.flight.estimateLines
                totalText: panel.flight.totalText
                fuelNote: panel.flight.fuelNote
            }
        }
    }

    Dialog {
        id: confirmCancel

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        padding: Theme.pagePadding

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radius
            border.color: Theme.border
            border.width: Theme.borderWidth
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacing

            Text {
                text: "Cancel " + panel.flight.title.replace(" · ", " ").replace(" → ", "–") + "? It's free."
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.gap8

                SecondaryButton {
                    text: "Keep it"
                    onClicked: confirmCancel.close()
                }

                PrimaryButton {
                    text: "Cancel flight"
                    onClicked: {
                        confirmCancel.close();
                        panel.flight.cancelFlight();
                    }
                }
            }
        }
    }
}
