// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Collects what milestone 10's analysis and the Pre-Flight panel need from SayIntentions (D-123, D-188): the ATC log
// about once a minute and once more at block in, the origin's ATIS every 5 minutes until liftoff, the destination's
// from top of descent to touchdown. At the gate (a flight planned, the sim connected, none open) ATC and the origin's
// ATIS are polled too, logged with flight_id "", so the clearance shows before pushback; block out logs the whole
// history again with the flight id. What's new goes onto the AppEventQueue, which the recorder writes to the event log
// (D-127). A failed call logs nothing and is simply tried again at the next poll.

#pragma once

#include <chrono>
#include <functional>
#include <optional>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include "app_event_queue.h"
#include "atc_log.h"
#include "gate_assigner.h"
#include "flight_match.h"
#include "planner.h"
#include "recorder_bridge.h"

class FlightWatcher : public QObject {
    Q_OBJECT

public:
    FlightWatcher(Planner& planner, RecorderBridge& bridge, const blocktime::FlightTargetSlot& target,
                  blocktime::AppEventQueue& queue, QObject* parent = nullptr);

    // 30 s and 5 min in the app (D-200); tests use a few milliseconds.
    void setIntervals(std::chrono::milliseconds atc, std::chrono::milliseconds atis);
    // The arrival gate (milestone 9, D-141): SayIntentions' flightJSON is polled this often (30 s in the app) from
    // block out until the gate is settled or block in.
    void setGateInterval(std::chrono::milliseconds gate);
    // What the airline knows for the destination (the type's wingspan, gate or ramp, preferred gates); nullopt = don't
    // pick a gate. Without it, no gate work is done.
    using GateContextFor = std::function<std::optional<blocktime::GateContext>(const QString& destinationIcao)>;
    void setGateContext(GateContextFor gateContext);
    // The planned flight's origin ("" = nothing planned). While one is planned, the sim is connected and no flight is
    // open, ATC is polled at the ATC interval and the origin's ATIS at the ATIS interval, logged with flight_id ""
    // (D-188), so the clearance shows before pushback.
    void setPreFlightOrigin(const QString& originIcao);

private:
    void onBlockEvent(const QString& type, const QString& flightId);
    void onFlightEvent(const QString& type, const QVariantMap& data);
    void fetchAtc();
    void fetchAtis(const QString& icao, const QString& role);
    void pollGate();
    void logGate(nlohmann::json event);
    void stopAll();
    void updatePreFlight();      // starts or stops the gate's polling (D-188)
    QString loggedFlightId() const;  // the open flight's id, "" at the gate

    Planner& planner_;
    RecorderBridge& bridge_;
    const blocktime::FlightTargetSlot& target_;
    blocktime::AppEventQueue& queue_;
    QTimer atcTimer_, originTimer_, destinationTimer_, gateTimer_;
    GateContextFor gateContext_;
    bool gateDone_ = false;  // one try per flight
    QString flightId_, origin_, destination_;
    bool destinationDone_ = false;  // touched down: no more destination ATIS this flight
    bool open_ = false;             // from block out to block in or an interrupt (flightId_ outlives it for block in)
    QString preFlightOrigin_;       // the planned flight's origin, "" when nothing is planned
    bool preFlight_ = false;        // polling at the gate
    blocktime::CommsDedupe comms_;
    blocktime::AtisDedupe atis_;
};
