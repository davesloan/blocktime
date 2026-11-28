// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The flight's life on the Live page (design.md §13.2-13.8): the planned flight, "Waiting for plan" until Dave imports
// the SimBrief plan with a button (D-101), then "Waiting for sim"; the route to copy, where to load in, the money
// estimate, and Cancel flight. It hands the planned flight to the recorder, says why a flight isn't being recorded
// (D-088), settles the flight at block in (D-113), keeps the plan after an interruption (D-108), and holds the
// summary until At Gate Turnaround or Done for now.

#pragma once

#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "flight_events.h"
#include "planned_flight.h"
#include "planned_flight_display.h"
#include "planner.h"
#include "recorder_bridge.h"
#include "settlement_coordinator.h"
#include "sqlite_db.h"

class PlannedFlightModel : public PlannedFlightDisplay {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")

public:
    // rates.yaml is read at every reload() for the estimate (D-107) and at every settle; the defaults if it can't be
    // read. `target` gets the planned flight for the recorder; `logDir` is where the flown flight is read back from.
    PlannedFlightModel(blocktime::db::Database& db, QString airlineIcao, Planner& planner, RecorderBridge& bridge,
                       blocktime::FlightTargetSlot& target, std::filesystem::path logDir, std::filesystem::path ratesFile,
                       QObject* parent = nullptr);

    // Reads the planned flight again; `message` replaces the shown message (planning's note about the runways).
    Q_INVOKABLE void reload(const QString& message = {});
    void openDispatchPage() override;
    void importPlan() override;
    void cancelFlight() override;
    // Settles the planned flight if the log shows it flown. The app calls it once at startup; block events call it.
    Q_INVOKABLE void checkFlown();
    // checkFlown() again if a flight was flown but couldn't be settled yet: its block_in line was held (D-049),
    // rates.yaml didn't read, or settling failed. The app's minute tick calls it (review 1.2).
    void retryPendingSettle();
    // The log lines the flight that just settled was measured from, for Claude's review (review 5.1: read once).
    // Empty after the first call.
    std::vector<blocktime::LogLine> takeSettledLines() { return std::exchange(settledLines_, {}); }
    void doneForNow() override;  // clears the summary
    void turnaround() override;  // clears the summary and asks for New Flight with the same airplane

    QString state() const override;
    QString notRecording() const override;

signals:
    void flightChanged();  // planned, imported or cancelled: the Fleet screen refreshes
    void settled(QString logFlightId);  // money, airplane and flights changed: Fleet, Money and Flights refresh
    void turnaroundRequested(QString registration);

private:
    blocktime::db::Database& db_;
    Planner& planner_;
    RecorderBridge& bridge_;
    void onBlockEvent(const QString& type);
    void settle(bool blockInSeen);

    blocktime::FlightTargetSlot& target_;
    std::filesystem::path logDir_;
    std::filesystem::path ratesFile_;
    int fuelOnRecordLb_ = 0;
    QString summaryRegistration_;
    blocktime::SettlementCoordinator settlement_;
    std::vector<blocktime::LogLine> settledLines_;  // until the settled handler takes them
};
