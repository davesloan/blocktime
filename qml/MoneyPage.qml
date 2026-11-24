// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airline's money: the cash, short and in full (D-080), and every ledger line, newest first.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property LedgerModel ledger

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.spacing

        StatTile {
            Layout.preferredWidth: Theme.cashTileWidth
            label: "Cash"
            value: page.ledger.cashShort
            unit: page.ledger.cashFull
        }

        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Ledger"

            Text {
                visible: lines.count === 0
                text: "No money has moved yet."
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            ListView {
                id: lines

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: page.ledger
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: line

                    required property string date
                    required property string text
                    required property string amount
                    required property bool income
                    required property int index

                    width: ListView.view.width
                    implicitHeight: Theme.ledgerRowHeight
                    color: index % 2 === 0 ? Theme.surface : Theme.surfaceRaised

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.gap12
                        anchors.rightMargin: Theme.gap12
                        spacing: Theme.spacing

                        Text {
                            Layout.preferredWidth: Theme.ledgerDateWidth
                            text: line.date
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            font.features: { "tnum": 1 }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: line.text
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                        }

                        Text {
                            text: line.amount
                            color: line.income ? Theme.good : Theme.textPrimary
                            horizontalAlignment: Text.AlignRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                            font.weight: Font.DemiBold
                            font.features: { "tnum": 1 }
                        }
                    }
                }
            }
        }
    }
}
