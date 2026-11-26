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
    required property FleetTableModel fleet
    required property LedgerModel ledger
    required property NewFlightModel newFlight
    required property PlannedFlightModel plannedFlight
    required property DashboardHeader dashboard
    required property PreFlightModel preflight
    required property InFlightModel inflight
    required property string airlineName
    property int startPage: 0  // --page; a developer option with --grab
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
                    recorder: window.recorder
                    fleet: window.fleet
                    plannedFlight: window.plannedFlight
                    onNewFlightRequested: registration => {
                        if (window.newFlight.open(registration)) newFlightDialog.open();
                    }
                }

                MoneyPage {
                    ledger: window.ledger
                }
            }
        }
    }
}
