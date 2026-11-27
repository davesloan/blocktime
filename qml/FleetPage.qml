// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Fleet screen (D-241, docs/fleet.md): the airline as a character sheet on the left (slots open, the next slot,
// hours, cash, leases, its notices) and its slots on the right: a card per airplane in lease order, the empty slots
// ("+ Add aircraft") and the locked ones with the hours that open them.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property FleetModel fleet

    signal flyRequested(string registration)
    signal continueRequested()
    signal manageRequested(string registration)
    signal addRequested()

    component Stat: RowLayout {
        id: stat

        required property string label
        required property string value

        Layout.fillWidth: true

        Text {
            Layout.fillWidth: true
            text: stat.label
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        Text {
            text: stat.value
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.spacing

        // The airline: the character.
        Rectangle {
            Layout.preferredWidth: Theme.fleetPanelWidth
            Layout.alignment: Qt.AlignTop
            implicitHeight: panel.implicitHeight + 2 * Theme.spacing
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth

            ColumnLayout {
                id: panel

                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: Theme.gap14

                RowLayout {
                    spacing: Theme.gap12

                    Rectangle {
                        implicitWidth: Theme.fleetLogoSize
                        implicitHeight: Theme.fleetLogoSize
                        radius: Theme.radius
                        color: Theme.accent

                        Text {
                            anchors.centerIn: parent
                            text: page.fleet.airlineCode
                            color: Theme.textOnAccent
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            font.weight: Font.Bold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gap2

                        Text {
                            Layout.fillWidth: true
                            text: page.fleet.airlineName
                            elide: Text.ElideRight
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeFleetTitle
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: page.fleet.airlineCode + " · " + page.fleet.callsign
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }

                ColumnLayout {
                    spacing: Theme.gap2

                    Text {
                        text: "Slots open"
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Text {
                        textFormat: Text.StyledText
                        text: page.fleet.slotsOpen + " <font color=\"" + Theme.textSecondary + "\">/ "
                              + page.fleet.slotsMax + "</font>"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeFleetLevel
                        font.weight: Font.Bold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gap6

                    Stat {
                        label: "Next slot"
                        value: page.fleet.nextSlotText
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Theme.progressBarHeight
                        radius: height / 2
                        color: Theme.surfaceRaised
                        border.color: Theme.border
                        border.width: Theme.borderWidth

                        Rectangle {
                            width: parent.width * page.fleet.nextSlotProgress
                            height: parent.height
                            radius: parent.radius
                            color: Theme.accent
                        }
                    }
                }

                Stat {
                    label: "Hours flown"
                    value: page.fleet.hoursText
                }

                Stat {
                    label: "Cash"
                    value: page.fleet.cashText
                }

                Stat {
                    label: "Leases / month"
                    value: page.fleet.leasesText
                }

                // The airline's notices, one line each: a missing config file, no airport data, an unpaid lease.
                Repeater {
                    model: page.fleet.notices

                    Text {
                        required property string modelData

                        Layout.fillWidth: true
                        text: modelData
                        color: Theme.warn
                        wrapMode: Text.WordWrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }

        // The slots: the gear.
        Flickable {
            id: slots

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: slotColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: slotColumn

                width: slots.width
                spacing: Theme.gap12

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.gap12

                    Repeater {
                        model: page.fleet

                        FleetCard {
                            onMainClicked: {
                                if (button === "Continue Flight") page.continueRequested();
                                else page.flyRequested(registration);
                            }
                            onManageClicked: page.manageRequested(registration)
                        }
                    }

                    Repeater {
                        model: page.fleet.emptySlots

                        Item {
                            implicitWidth: Theme.fleetCardWidth
                            implicitHeight: Theme.fleetCardHeight

                            Shape {
                                anchors.fill: parent
                                preferredRendererType: Shape.CurveRenderer

                                ShapePath {
                                    strokeColor: emptyArea.containsMouse ? Theme.accent : Theme.slotEmptyBorder
                                    strokeWidth: Theme.emptySlotBorderWidth
                                    strokeStyle: ShapePath.DashLine
                                    dashPattern: [Theme.dashLength, Theme.dashLength]
                                    fillColor: "transparent"

                                    PathRectangle {
                                        x: Theme.emptySlotBorderWidth / 2
                                        y: Theme.emptySlotBorderWidth / 2
                                        width: Theme.fleetCardWidth - Theme.emptySlotBorderWidth
                                        height: Theme.fleetCardHeight - Theme.emptySlotBorderWidth
                                        radius: Theme.radius
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Theme.gap4

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "+"
                                    color: emptyArea.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizePlus
                                    font.weight: Font.Light
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "Add aircraft"
                                    color: emptyArea.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeBody
                                }
                            }

                            MouseArea {
                                id: emptyArea

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: page.addRequested()
                            }
                        }
                    }
                }

                // The locked slots: the hours that open them, the next one amber; then how many more.
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.gap8
                    visible: page.fleet.lockedSlotHours.length > 0

                    Repeater {
                        model: page.fleet.lockedSlotHours.slice(0, Theme.lockedTilesShown)

                        Rectangle {
                            id: tile

                            required property int modelData
                            required property int index

                            implicitWidth: Theme.lockedTileWidth
                            implicitHeight: Theme.lockedTileHeight
                            radius: Theme.radius
                            color: Theme.slotLockedBg
                            border.color: tile.index === 0 ? Theme.slotNextBorder : Theme.slotLockedBorder
                            border.width: Theme.borderWidth

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Theme.gap2

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: ""  // Lock
                                    color: tile.index === 0 ? Theme.warn : Theme.textMuted
                                    font.family: Theme.iconFontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: tile.modelData + " h"
                                    color: tile.index === 0 ? Theme.warn : Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: page.fleet.lockedSlotHours.length > Theme.lockedTilesShown
                        implicitWidth: Theme.lockedTileWidth
                        implicitHeight: Theme.lockedTileHeight
                        radius: Theme.radius
                        color: Theme.slotLockedBg
                        border.color: Theme.slotLockedBorder
                        border.width: Theme.borderWidth

                        Text {
                            anchors.centerIn: parent
                            text: "+" + (page.fleet.lockedSlotHours.length - Theme.lockedTilesShown) + " more"
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }
        }
    }
}
