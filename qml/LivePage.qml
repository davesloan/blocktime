// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The live flight screen: the planned flight before departure (design.md §13.3), the flight panel and the event
// feed. The page scrolls when the window is too short for all of it; the scroll bar shows only then.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property RecorderBridge recorder
    required property PlannedFlightModel plannedFlight

    ScrollView {
        id: scroll

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        contentHeight: column.height + 2 * Theme.pagePadding
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: contentHeight > availableHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

        ColumnLayout {
            id: column

            x: Theme.pagePadding
            y: Theme.pagePadding
            width: scroll.availableWidth - 2 * Theme.pagePadding
            // Fill the window when there's room; otherwise take the height everything needs and scroll.
            height: Math.max(scroll.availableHeight - 2 * Theme.pagePadding, implicitHeight)
            spacing: Theme.spacing

            PlannedFlightPanel {
                Layout.fillWidth: true
                visible: page.plannedFlight.hasFlight
                flight: page.plannedFlight
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: flightPanel.implicitHeight
                spacing: Theme.spacing

                FlightPanel {
                    id: flightPanel

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
}
