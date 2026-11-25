// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One settled flight (design.md §13.7): its times, each income and expense line with the total, and the route on the
// map. Live shows it after block in with At Gate Turnaround and Done for now; Flights shows it for the selected
// flight, without the buttons. The score and landing stars sit in the header, and the Debrief button switches to the
// debrief (docs/debrief.md section 6) in place of the times, money and map.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Card {
    id: card

    // FlightListModel.summary(row): { title, dateText, outText, inText, blockText, fuelText, routeText, income, costs,
    // totalText, parkedText, flightId, debrief }
    required property var summary
    required property var track  // FlightListModel.track(row)
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent
    required property ReviewWriter reviews
    property bool showActions: false
    property bool showDebrief: false
    // The Replay button (D-175): on Flights and on Live, not in the replay window itself.
    property bool replayable: false

    readonly property var debrief: card.summary.debrief ?? ({})
    readonly property bool measured: card.debrief.measured ?? false

    signal turnaround()
    signal done()
    signal replay()

    title: card.summary.title ?? ""

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.gap12

        Text {
            Layout.fillWidth: true
            text: (card.summary.dateText ?? "") + " · " + (card.summary.routeText ?? "")
                  + (card.summary.parkedText ? " · " + card.summary.parkedText : "")
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        Text {
            visible: card.measured
            text: "Score " + (card.debrief.scoreText ?? "")
                  + (card.debrief.starsText ? " · Landing " + card.debrief.starsText : "")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
        }

        SecondaryButton {
            visible: card.measured
            text: card.showDebrief ? "Flight" : "Debrief"
            onClicked: card.showDebrief = !card.showDebrief
        }

        // Why Replay is greyed: the log is kept 30 days.
        Text {
            visible: card.replayable && (card.summary.replayNote ?? "") !== ""
            text: card.summary.replayNote ?? ""
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }

        SecondaryButton {
            visible: card.replayable
            enabled: (card.summary.replayNote ?? "") === ""
            text: "Replay"
            onClicked: card.replay()
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

    DebriefPanel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: card.measured && card.showDebrief
        debrief: card.debrief
        flightId: card.summary.flightId ?? ""
        reviews: card.reviews
    }

    GridLayout {
        Layout.fillWidth: true
        visible: !(card.measured && card.showDebrief)
        columns: 4
        columnSpacing: Theme.gap12
        rowSpacing: Theme.gap12

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
        Layout.minimumHeight: Theme.summaryMapMinHeight
        visible: !(card.measured && card.showDebrief)
        spacing: Theme.spacing

        MoneyLines {
            Layout.fillWidth: false  // a layout fills by default, which would squeeze the map out
            Layout.preferredWidth: Theme.sideColumnWidth
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
            border.width: Theme.borderWidth
            track: card.track
            tileUrl: card.mapTileUrl
            cacheDir: card.mapCacheDir
            userAgent: card.mapUserAgent
        }
    }
}
