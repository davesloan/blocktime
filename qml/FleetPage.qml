// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airline's airplanes from the database, with the airline's notices above them (a missing config file, no
// airport data, a lease that couldn't be paid). Click a column header to sort by it; click it again to reverse.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property RecorderBridge recorder
    required property FleetTableModel fleet

    // Relative column widths: Registration, Type, Airframe, Fleet, Location, Fuel, Status.
    readonly property var columnWeights: [1.1, 1.2, 1.5, 1.0, 0.8, 0.9, 1.4]
    readonly property real weightSum: columnWeights.reduce((a, b) => a + b, 0)
    readonly property int fuelColumn: 5
    readonly property int statusColumn: 6

    Card {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        title: "Fleet · " + table.rows + (table.rows === 1 ? " airplane" : " airplanes")

        // The airline's notices, one line each.
        Repeater {
            model: page.fleet.notices

            Text {
                required property string modelData

                Layout.fillWidth: true
                text: modelData
                color: Theme.warn
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }
        }

        Text {
            visible: table.rows === 0 && page.fleet.notices.length === 0
            text: "No airplanes yet."
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        HorizontalHeaderView {
            id: header

            Layout.fillWidth: true
            Layout.preferredHeight: 36
            syncView: table
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: headerCell

                required property string display
                required property int column

                readonly property bool sorted: page.fleet.sortColumn === column

                implicitHeight: 36
                color: "transparent"

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.border
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: headerCell.column === page.fuelColumn ? Text.AlignRight : Text.AlignLeft
                    text: headerCell.display.toUpperCase()
                          + (headerCell.sorted ? (page.fleet.sortOrder === Qt.AscendingOrder ? "  ▲" : "  ▼") : "")
                    color: headerCell.sorted ? Theme.textPrimary : Theme.textSecondary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.labelLetterSpacing
                }

                TapHandler {
                    onTapped: page.fleet.sort(headerCell.column,
                                              headerCell.sorted && page.fleet.sortOrder === Qt.AscendingOrder
                                              ? Qt.DescendingOrder : Qt.AscendingOrder)
                }
            }
        }

        TableView {
            id: table

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: page.fleet
            columnWidthProvider: column => Math.floor(table.width * page.columnWeights[column] / page.weightSum)
            onWidthChanged: forceLayout()
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: cell

                required property string display
                required property int row
                required property int column

                readonly property bool isStatus: column === page.statusColumn
                readonly property bool isNumber: column === page.fuelColumn
                readonly property color tone: display === "Parked" ? Theme.good
                                            : display.startsWith("Returned") ? Theme.bad : Theme.accent

                implicitHeight: 44
                color: row % 2 === 0 ? Theme.surface : Theme.surfaceRaised

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    visible: !cell.isStatus
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: cell.isNumber ? Text.AlignRight : Text.AlignLeft
                    text: cell.display
                    color: cell.column === 0 ? Theme.textPrimary : Theme.textSecondary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: cell.column === 0 ? Font.DemiBold : Font.Normal
                    font.features: { "tnum": 1 }
                }

                // Status as a small pill: Parked, Awaiting delivery, or Returned (the lease ended).
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    visible: cell.isStatus
                    width: statusText.implicitWidth + 16
                    height: 22
                    radius: 11
                    color: Qt.rgba(cell.tone.r, cell.tone.g, cell.tone.b, 0.12)
                    border.color: Qt.rgba(cell.tone.r, cell.tone.g, cell.tone.b, 0.35)
                    border.width: 1

                    Text {
                        id: statusText

                        anchors.centerIn: parent
                        text: cell.display
                        color: cell.tone
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }
}
