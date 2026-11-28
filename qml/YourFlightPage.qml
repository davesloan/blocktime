// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Your Flight (docs/dispatch.md, D-233): one thing per stage. The dispatch form, then the dispatch report until
// Accept; then the dashboard header with Pre-Flight, the in-flight panel from block out, and the summary after Settle
// Up (§13.7). "Not recording" (design.md §13.4) and the notice stay on top; with --dev (D-211) the flight panel and the
// event feed sit under it all. The page scrolls when the window is too short; the scroll bar shows only then.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property RecorderBridge recorder
    // The flight the panels show: Your Flight's, or the replay's (D-175).
    required property FlightSource flightSource
    // Your Flight's own models; null in the replay window.
    property YourFlightModel yourFlight: null
    property DispatchModel dispatch: null
    required property DashboardHeader dashboard
    required property PreFlightModel preflight
    required property InFlightModel inflight
    required property FlightListModel flights
    required property ReviewWriter reviews
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent
    // The replay window's Live (D-175): the summary at block in has no At Gate Turnaround or Done for now; the
    // replay's bar has Replay again and Close.
    property bool replaying: false
    // Developer mode (--dev, D-211): the flight tiles and the event feed under the panels; hidden otherwise.
    property bool devMode: false

    // The summary of the flight that just ended, from the Flights list (design.md §13.7).
    // From Finish Flight (0.15.1): until then the arrival view stays; a flight settled at startup shows it at once.
    readonly property bool showSummary: flightSource.stage === "summary"
    // Dispatch until Accept (D-233). Never beside the dashboard: --demo shows its made-up flight's dashboard instead.
    readonly property bool dispatching: dispatch !== null && !dashboard.visible
    readonly property bool showForm: dispatching && (flightSource.stage === "empty" || flightSource.stage === "form")
    readonly property bool showReport: dispatching
                                       && (flightSource.stage === "dispatched" || flightSource.stage === "imported")

    signal fleetRequested()
    property var summary: ({})
    property var summaryTrack

    function refreshSummary() {
        const row = page.showSummary ? page.flights.rowOf(page.flightSource.summaryFlightId) : -1
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

            // The recorder's own problem first: events that can't reach the log matter more than a mismatch (review 1.8).
            NotRecordingBanner {
                Layout.fillWidth: true
                text: page.recorder.recorderProblem !== "" ? page.recorder.recorderProblem
                     : page.yourFlight !== null ? page.yourFlight.notRecording
                     : ""
            }

            // A flight that was interrupted and discarded, or one that couldn't be settled (D-072, D-108).
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: page.yourFlight !== null ? page.yourFlight.notice : ""
                color: Theme.warn
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            // The dispatch form: empty (no airplane chosen), pre-filled from Fleet's Fly, or Change flight.
            Loader {
                Layout.preferredWidth: Theme.formWidth
                Layout.maximumWidth: column.width
                visible: active
                active: page.showForm

                sourceComponent: DispatchForm {
                    model: page.dispatch
                    onFleetRequested: page.fleetRequested()
                }
            }

            // The dispatch report, until Accept (D-234).
            Loader {
                Layout.fillWidth: true
                visible: active
                active: page.showReport

                sourceComponent: DispatchReport {
                    model: page.dispatch
                }
            }

            // The dashboard header (D-151): from Accept until the summary is done with (D-233).
            FlightHeader {
                Layout.fillWidth: true
                visible: page.dashboard.visible
                header: page.dashboard
                recorder: page.recorder
                reportAvailable: page.dispatch !== null && page.flightSource.stage !== "summary"
                onReportRequested: reportPopup.active = true
            }

            // The Pre-Flight panel (D-153): from Accept until block out (D-194), right under the header.
            PreFlightPanel {
                Layout.fillWidth: true
                visible: page.preflight.visible
                model: page.preflight
            }

            // The in-flight panel (D-194): from block out to block in, where the Pre-Flight panel was.
            InFlightPanel {
                Layout.fillWidth: true
                visible: page.inflight.visible
                model: page.inflight
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
                    showActions: !page.replaying
                    replayable: !page.replaying
                    onTurnaround: page.yourFlight?.turnaround()
                    onDone: page.yourFlight?.doneForNow()
                    onReplay: page.flights.replay(page.flightSource.summaryFlightId)
                }
            }

            // Takes the height left over, so everything above stays at the top (the --dev row does it in dev mode).
            Item {
                Layout.fillHeight: true
                visible: !page.devMode
            }

            // The flight tiles and the event log, only in developer mode (--dev, D-211). The header's pill and status
            // lines replace the old stages column.
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: page.devMode ? flightPanel.implicitHeight : 0
                visible: page.devMode
                spacing: Theme.spacing

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

    // View Dispatch Report: the accepted report in an overlay, built only while it's open.
    Loader {
        id: reportPopup

        active: false

        sourceComponent: DispatchReportPopup {
            model: page.dispatch
            parent: page
            onClosed: reportPopup.active = false
            Component.onCompleted: open()
        }
    }
}
