// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The shell: the side menu on the left, and on the right a header above the current page (D-062).
ApplicationWindow {
    id: window

    required property RecorderBridge recorder
    required property FlightListModel flights
    required property ReviewWriter reviews
    required property FleetModel fleet
    required property AddAircraftModel addAircraft
    required property AircraftModel aircraft
    required property LedgerModel ledger
    required property NewFlightModel newFlight
    required property PlannedFlightModel plannedFlight
    required property DashboardHeader dashboard
    required property PreFlightModel preflight
    required property InFlightModel inflight
    required property string airlineName
    property int startPage: 0  // --page; a developer option with --grab
    property bool devMode: false  // --dev (D-211): Live shows the flight tiles and the event feed
    // False for --demo and --grab, which record nothing (D-226): their made-up or absent flight needs no guard.
    property bool recording: true
    // Map settings from app_main (D-059). The tile URL holds the CARTO key: never shown or logged.
    required property string mapTileUrl
    required property string mapCacheDir
    required property string mapUserAgent

    // 200 px wider than test 1, so the Live page keeps test 1's width beside the side menu.
    width: Theme.windowWidth
    height: Theme.windowHeight
    minimumWidth: Theme.windowMinWidth
    minimumHeight: Theme.windowMinHeight
    visible: true
    title: "BlockTime"
    color: Theme.bg

    // New Flight (design.md §13.1). Once the flight is planned, the Live page shows it.
    // At Gate Turnaround: New Flight for the same airplane at its new airport (design.md §13.7).
    Connections {
        target: window.plannedFlight

        function onTurnaroundRequested(registration: string) {
            if (window.newFlight.open(registration)) newFlightDialog.open();
        }
    }

    // Closing while a flight is open asks first (review 1.9): the flight would be discarded.
    property bool closeConfirmed: false

    onClosing: close => {
        // Held events (D-227) are lost too: a block in whose line isn't written yet never settles.
        const held = window.recorder.recorderProblem !== "";
        if (window.recording && (window.recorder.flightOpen || held) && !window.closeConfirmed) {
            close.accepted = false;
            closeFlightDialog.eventsHeld = held && !window.recorder.flightOpen;
            closeFlightDialog.open();
        }
    }

    CloseFlightDialog {
        id: closeFlightDialog

        onCloseAnyway: {
            window.closeConfirmed = true;
            window.close();
        }
    }

    NewFlightDialog {
        id: newFlightDialog

        flight: window.newFlight
        onFlightPlanned: sidebar.currentIndex = 0
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar

            currentIndex: window.startPage

            airlineName: window.airlineName

            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Header: page name, connection pill, Open log folder
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.headerHeight
                color: Theme.surface

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Theme.hairline
                    color: Theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.pagePadding
                    anchors.rightMargin: Theme.pagePadding
                    spacing: Theme.gap14

                    Text {
                        Layout.fillWidth: true
                        text: sidebar.pageNames[sidebar.currentIndex]
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
                    }

                    StatusPill {
                        status: window.recorder.connection
                    }

                    PrimaryButton {
                        text: "Open log folder"
                        enabled: window.recorder.logDir !== ""
                        onClicked: window.recorder.openLogFolder()
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: sidebar.currentIndex

                LivePage {
                    recorder: window.recorder
                    plannedFlight: window.plannedFlight
                    dashboard: window.dashboard
                    preflight: window.preflight
                    inflight: window.inflight
                    flights: window.flights
                    reviews: window.reviews
                    devMode: window.devMode
                    mapTileUrl: window.mapTileUrl
                    mapCacheDir: window.mapCacheDir
                    mapUserAgent: window.mapUserAgent
                }

                FlightsPage {
                    recorder: window.recorder
                    flights: window.flights
                    reviews: window.reviews
                    mapTileUrl: window.mapTileUrl
                    mapCacheDir: window.mapCacheDir
                    mapUserAgent: window.mapUserAgent
                }

                FleetPage {
                    fleet: window.fleet
                    addAircraft: window.addAircraft
                    aircraft: window.aircraft
                    onFlyRequested: registration => {
                        if (window.newFlight.open(registration)) newFlightDialog.open();
                    }
                    onContinueRequested: sidebar.currentIndex = 0
                }

                MoneyPage {
                    ledger: window.ledger
                }
            }
        }
    }
}
