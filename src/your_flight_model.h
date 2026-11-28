// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Your Flight (docs/dispatch.md, D-233): the planned flight and the stage the page shows, from the form to the
// summary. It hands the planned flight to the recorder, says why a flight isn't being recorded (D-088), settles the
// flight at block in (D-113, through SettlementCoordinator), keeps the plan after an interruption (D-108), and holds
// the summary until At Gate Turnaround or Done for now. The form and the dispatch report are DispatchModel's.

#pragma once

#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "flight_events.h"
#include "flight_source.h"
#include "planned_flight.h"
#include "recorder_bridge.h"
#include "settlement_coordinator.h"
#include "sqlite_db.h"

class YourFlightModel : public FlightSource {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(bool hasFlight READ hasFlight NOTIFY changed)
    Q_PROPERTY(QString notRecording READ notRecording NOTIFY notRecordingChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    // rates.yaml is read at every settle of a flight that wasn't accepted. `target` gets the planned flight for the
    // recorder; `logDir` is where the flown flight is read back from.
    YourFlightModel(blocktime::db::Database& db, RecorderBridge& bridge, blocktime::FlightTargetSlot& target,
                    std::filesystem::path logDir, std::filesystem::path ratesFile, QObject* parent = nullptr);

    // Reads the planned flight again and hands it to the recorder; `message` replaces the shown message.
    Q_INVOKABLE void reload(const QString& message = {});
    // Not while it's being flown, nor once flown and waiting to settle (review 1.2); `message` says why.
    void cancelFlight();
    // Settles the planned flight if the log shows it flown. The app calls it once at startup; block events call it.
    Q_INVOKABLE void checkFlown();
    // checkFlown() again if a flight was flown but couldn't be settled yet: its block_in line was held (D-049),
    // rates.yaml didn't read, or settling failed. The app's minute tick calls it (review 1.2).
    void retryPendingSettle();
    // The log lines the flight that just settled was measured from, for Claude's review (review 5.1: read once).
    // Empty after the first call.
    std::vector<blocktime::LogLine> takeSettledLines() { return std::exchange(settledLines_, {}); }
    Q_INVOKABLE void doneForNow();  // clears the summary
    Q_INVOKABLE void turnaround();  // clears the summary and asks for the form with the same airplane
    // The dispatch form is open (a new flight, or Change flight).
    Q_INVOKABLE void setFormOpen(bool open);

    const std::optional<blocktime::PlannedFlightView>& view() const override { return flight_; }
    blocktime::YourFlightStage stage() const override;
    QString summaryFlightId() const override { return summaryFlightId_; }
    bool hasFlight() const { return flight_.has_value(); }
    QString notRecording() const;
    QString notice() const { return notice_; }
    QString message() const { return message_; }

signals:
    void notRecordingChanged();
    void flightChanged();  // planned, imported or cancelled: the Fleet screen refreshes
    void settled(QString logFlightId);  // money, airplane and flights changed: Fleet, Money and Flights refresh
    void turnaroundRequested(QString registration);

private:
    void onBlockEvent(const QString& type);
    void settle(bool blockInSeen);

    blocktime::db::Database& db_;
    RecorderBridge& bridge_;
    blocktime::FlightTargetSlot& target_;
    std::filesystem::path logDir_;
    std::filesystem::path ratesFile_;
    std::optional<blocktime::PlannedFlightView> flight_;
    QString message_;
    QString notice_;
    QString summaryFlightId_;
    int fuelOnRecordLb_ = 0;
    QString summaryRegistration_;
    blocktime::SettlementCoordinator settlement_;
    bool formOpen_ = false;
    std::vector<blocktime::LogLine> settledLines_;  // until the settled handler takes them
};
