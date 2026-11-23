// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The live flight screen: the planned flight before departure (design.md §13.3), the flight panel and the event
// feed.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Item {
    id: page

    required property RecorderBridge recorder
    required property PlannedFlightModel plannedFlight

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.spacing

        PlannedFlightPanel {
            Layout.fillWidth: true
            visible: page.plannedFlight.hasFlight
            flight: page.plannedFlight
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing

            FlightPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                recorder: page.recorder
            }

            EventFeed {
                Layout.fillHeight: true
                Layout.preferredWidth: 360
                recorder: page.recorder
            }
        }
    }
}
