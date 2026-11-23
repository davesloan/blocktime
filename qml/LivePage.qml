// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The live flight screen: "Not recording" when the sim doesn't match the plan (design.md §13.4), the planned flight
// before and during departure (§13.3), the summary after block in (§13.7), the flight panel and the event feed. The
// page scrolls when the window is too short for all of it; the scroll bar shows only then.

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

            NotRecordingBanner {
                Layout.fillWidth: true
                text: page.plannedFlight.notRecording
            }

            // A flight that was interrupted and discarded, or one that couldn't be settled (D-072, D-108).
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: page.plannedFlight.notice
                color: Theme.warn
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            // The summary of the flight that just ended; FlightSummary replaces this placeholder (milestone 6, Task 8).
            Text {
                Layout.fillWidth: true
                visible: page.plannedFlight.state === "summary"
                text: "Flight " + page.plannedFlight.summaryFlightId + " settled."
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

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
