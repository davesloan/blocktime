// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One settled flight (design.md §13.7): its times, each income and expense line with the total, and the route on the
// map. Live shows it after block in with At Gate Turnaround and Done for now; Flights shows it for the selected
// flight, without the buttons. No score until milestone 10.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Card {
    id: card

    // FlightListModel.summary(row): { title, dateText, outText, inText, blockText, fuelText, routeText, income, costs,
    // totalText }
    required property var summary
    required property var track  // FlightListModel.track(row)
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent
    property bool showActions: false

    signal turnaround()
    signal done()

    title: card.summary.title ?? ""

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: (card.summary.dateText ?? "") + " · " + (card.summary.routeText ?? "")
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        SecondaryButton {
            visible: card.showActions
            text: "Done for now"
            onClicked: card.done()
        }

        PrimaryButton {
            visible: card.showActions
            text: "At Gate Turnaround"
            onClicked: card.turnaround()
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: 12
        rowSpacing: 12

        StatTile {
            Layout.fillWidth: true
            label: "Block out"
            value: card.summary.outText ?? "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "Block in"
            value: card.summary.inText ?? "—"
        }
        StatTile {
            Layout.fillWidth: true
            label: "Block time"
            value: card.summary.blockText ?? "—"
            accent: Theme.accent
        }
        StatTile {
            Layout.fillWidth: true
            label: "Fuel used"
            value: (card.summary.fuelText ?? "—").replace(" lb used", "")
            unit: "lb"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 240
        spacing: Theme.spacing

        MoneyLines {
            Layout.fillWidth: false  // a layout fills by default, which would squeeze the map out
            Layout.preferredWidth: 340
            Layout.alignment: Qt.AlignTop
            income: card.summary.income ?? []
            costs: card.summary.costs ?? []
            totalText: card.summary.totalText ?? ""
        }

        RouteMap {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1
            track: card.track
            tileUrl: card.mapTileUrl
            cacheDir: card.mapCacheDir
            userAgent: card.mapUserAgent
        }
    }
}
