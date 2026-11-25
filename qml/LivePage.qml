// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The live flight screen: "Not recording" when the sim doesn't match the plan (design.md §13.4), the planned flight
// before and during departure (§13.3), the summary after block in (§13.7), the flight panel and the event feed. The
// page scrolls when the window is too short for all of it; the scroll bar shows only then.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property RecorderBridge recorder
    required property PlannedFlightModel plannedFlight
    required property FlightListModel flights
    required property ReviewWriter reviews
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent

    // The summary of the flight that just ended, from the Flights list (design.md §13.7).
    readonly property bool showSummary: plannedFlight.state === "summary"
    property var summary: ({})
    property var summaryTrack

    function refreshSummary() {
        const row = page.showSummary ? page.flights.rowOf(page.plannedFlight.summaryFlightId) : -1
        page.summary = row >= 0 ? page.flights.summary(row) : ({})
        page.summaryTrack = row >= 0 ? page.flights.track(row) : undefined
    }

    onShowSummaryChanged: refreshSummary()
    // A flight settled at startup is already showing its summary when the page is built.
    Component.onCompleted: refreshSummary()

    Connections {
        target: page.flights

        function onModelReset() { page.refreshSummary() }
        function onTracksChanged() { page.refreshSummary() }
        function onDebriefChanged() { page.refreshSummary() }
    }

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

            // Only built while it shows, so the map costs nothing the rest of the time.
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.summaryHeight
                visible: active
                active: page.showSummary

                sourceComponent: FlightSummary {
                    summary: page.summary
                    track: page.summaryTrack
                    reviews: page.reviews
                    mapTileUrl: page.mapTileUrl
                    mapCacheDir: page.mapCacheDir
                    mapUserAgent: page.mapUserAgent
                    showActions: true
                    onTurnaround: page.plannedFlight.turnaround()
                    onDone: page.plannedFlight.doneForNow()
                }
            }

            PlannedFlightPanel {
                Layout.fillWidth: true
                visible: page.plannedFlight.hasFlight
                flight: page.plannedFlight
            }

            // Three columns (D-139): Flight progress, the flight panel, the event log.
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Math.max(flightPanel.implicitHeight, progress.implicitHeight)
                spacing: Theme.spacing

                // The six stages across the gate (design.md §13.5, D-134): the flight from block out until its summary
                // is dismissed, otherwise the next leg with its GSX services so far.
                FlightProgress {
                    id: progress

                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: false
                    Layout.preferredWidth: Theme.sideColumnWidth
                    Layout.maximumWidth: Theme.sideColumnWidth
                    visible: page.recorder.showingFlight ? (page.recorder.flightOpen || page.showSummary)
                        : (page.plannedFlight.hasFlight || page.recorder.hasNextLegServices)
                    recorder: page.recorder
                }

                FlightPanel {
                    id: flightPanel

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    recorder: page.recorder
                }

                EventFeed {
                    Layout.fillHeight: true
                    Layout.fillWidth: false
                    Layout.preferredWidth: Theme.liveFeedWidth
                    Layout.maximumWidth: Theme.liveFeedWidth
                    recorder: page.recorder
                }
            }
        }
    }
}
