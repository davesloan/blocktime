// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dispatch on Your Flight (docs/dispatch.md, D-233, D-234): the form (a FlightDraft), Dispatch, the dispatch report
// (the ring, the split-flap board, the working, the load and the plan), Import with its check, Accept and Cancel.
// Every number and text comes from core (dispatch_report.h, simbrief.h, accepted_breakdown.h); QML only draws.

#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <set>
#include <string>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "app_event_queue.h"
#include "dispatch_report.h"
#include "flight_draft.h"
#include "planner.h"
#include "recorder_bridge.h"
#include "sqlite_db.h"
#include "your_flight_model.h"

class DispatchModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    // The form.
    Q_PROPERTY(bool formReady READ formReady NOTIFY changed)  // an airplane is chosen
    Q_PROPERTY(bool changing READ changing NOTIFY changed)    // Change flight, not a new one
    Q_PROPERTY(QString registration READ registration NOTIFY changed)
    Q_PROPERTY(QString airplaneText READ airplaneText NOTIFY changed)
    Q_PROPERTY(QString origin READ origin NOTIFY changed)
    Q_PROPERTY(QStringList kinds READ kinds NOTIFY changed)
    Q_PROPERTY(QStringList kindLabels READ kindLabels NOTIFY changed)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY changed)
    Q_PROPERTY(bool fullyLoadedAvailable READ fullyLoadedAvailable NOTIFY changed)
    Q_PROPERTY(bool fullyLoaded READ fullyLoaded WRITE setFullyLoaded NOTIFY changed)
    Q_PROPERTY(bool bellyCargoAvailable READ bellyCargoAvailable NOTIFY changed)
    Q_PROPERTY(bool bellyCargo READ bellyCargo WRITE setBellyCargo NOTIFY changed)
    Q_PROPERTY(QString destination READ destination WRITE setDestination NOTIFY changed)
    Q_PROPERTY(QString destinationMessage READ destinationMessage NOTIFY changed)
    Q_PROPERTY(QString flightNumber READ flightNumber WRITE setFlightNumber NOTIFY changed)
    Q_PROPERTY(QString additionalFuel READ additionalFuel WRITE setAdditionalFuel NOTIFY changed)
    Q_PROPERTY(QStringList loadLines READ loadLines NOTIFY changed)
    Q_PROPERTY(bool canDispatch READ canDispatch NOTIFY changed)
    // The report.
    Q_PROPERTY(QString reportTitle READ reportTitle NOTIFY changed)
    Q_PROPERTY(QString extraFuelBadge READ extraFuelBadge NOTIFY changed)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY changed)
    Q_PROPERTY(bool imported READ imported NOTIFY changed)
    Q_PROPERTY(bool accepted READ accepted NOTIFY changed)
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(QVariantList boardRows READ boardRows NOTIFY changed)
    Q_PROPERTY(int importCount READ importCount NOTIFY changed)  // bumped at each import: the board flips once
    Q_PROPERTY(QVariantMap ring READ ring NOTIFY changed)
    Q_PROPERTY(QString centerLabel READ centerLabel NOTIFY changed)
    Q_PROPERTY(QString centerValue READ centerValue NOTIFY changed)
    Q_PROPERTY(QString centerSub READ centerSub NOTIFY changed)
    Q_PROPERTY(QString centerTone READ centerTone NOTIFY changed)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(QVariantMap selection READ selection NOTIFY changed)
    Q_PROPERTY(QString hint READ hint NOTIFY changed)
    Q_PROPERTY(QString loadTitle READ loadTitle NOTIFY changed)
    Q_PROPERTY(int seatsFilled READ seatsFilled NOTIFY changed)
    Q_PROPERTY(int seatsTotal READ seatsTotal NOTIFY changed)
    // The seat strip: one segment a seat, at most 60 (a 777's seats would be hairlines).
    Q_PROPERTY(int seatSegments READ seatSegments NOTIFY changed)
    Q_PROPERTY(int seatSegmentsFilled READ seatSegmentsFilled NOTIFY changed)
    Q_PROPERTY(QString loadText READ loadText NOTIFY changed)
    Q_PROPERTY(QVariantList weightBars READ weightBars NOTIFY changed)
    Q_PROPERTY(QString planTitle READ planTitle NOTIFY changed)
    Q_PROPERTY(QString originIcao READ originIcao NOTIFY changed)
    Q_PROPERTY(QString destinationIcao READ destinationIcao NOTIFY changed)
    Q_PROPERTY(QString departureText READ departureText NOTIFY changed)
    Q_PROPERTY(QString arrivalText READ arrivalText NOTIFY changed)
    Q_PROPERTY(QVariantList planCells READ planCells NOTIFY changed)
    Q_PROPERTY(QString routeForCharts READ routeForCharts NOTIFY changed)
    Q_PROPERTY(QString openSimbriefLabel READ openSimbriefLabel NOTIFY changed)
    Q_PROPERTY(QString importLabel READ importLabel NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    // rates.yaml is read when the form opens (D-107) and for the report before Accept; after Accept the report is the
    // stored breakdown (D-222).
    DispatchModel(blocktime::db::Database& db, QString airlineIcao, std::filesystem::path ratesFile, Planner& planner,
                  RecorderBridge& bridge, blocktime::AppEventQueue& appEvents, YourFlightModel& yourFlight,
                  QObject* parent = nullptr);

    // The form for the airplane (Fleet's Fly, At Gate Turnaround): rolls its loads now. False if it can't fly, or
    // another flight is planned.
    Q_INVOKABLE bool openForAirplane(const QString& registration);
    Q_INVOKABLE void openEmpty();  // the form with no airplane
    Q_INVOKABLE void closeForm();  // back to the report (Change flight), or to nothing
    // Fetches the runways, then writes the planned flight (or, in Change flight, rewrites it) and shows the report.
    Q_INVOKABLE void dispatch();
    Q_INVOKABLE void changeFlight();  // back to the form with this flight's loads; before Accept only
    Q_INVOKABLE void saveChange() { dispatch(); }
    Q_INVOKABLE void openSimbrief();
    Q_INVOKABLE void importPlan();
    Q_INVOKABLE void accept();
    Q_INVOKABLE void cancelFlight();
    Q_INVOKABLE void select(const QString& id);  // "" clears it; selecting the selected line clears it too
    Q_INVOKABLE void copyRoute();

    bool formReady() const { return draft_.has_value(); }
    bool changing() const { return changing_; }
    QString registration() const;
    QString airplaneText() const;
    QString origin() const;
    QStringList kinds() const;
    QStringList kindLabels() const;
    QString kind() const;
    void setKind(const QString& code);
    bool fullyLoadedAvailable() const { return draft_ && draft_->fullyLoadedAvailable(); }
    bool fullyLoaded() const { return draft_ && draft_->fullyLoaded(); }
    void setFullyLoaded(bool on);
    bool bellyCargoAvailable() const { return draft_ && draft_->bellyCargoAvailable(); }
    bool bellyCargo() const { return draft_ && draft_->bellyCargo(); }
    void setBellyCargo(bool on);
    QString destination() const { return destination_; }
    void setDestination(const QString& typed);
    QString destinationMessage() const;
    QString flightNumber() const;
    void setFlightNumber(const QString& number);
    QString additionalFuel() const { return additionalFuel_; }
    void setAdditionalFuel(const QString& text);  // digits only; "" = none
    QStringList loadLines() const;
    bool canDispatch() const;

    QString reportTitle() const;
    QString extraFuelBadge() const;
    QStringList warnings() const;
    bool imported() const;
    bool accepted() const;
    bool locked() const;
    QVariantList boardRows() const;
    int importCount() const { return importCount_; }
    QVariantMap ring() const;
    QString centerLabel() const;
    QString centerValue() const;
    QString centerSub() const;
    QString centerTone() const;
    QString selectedId() const { return selectedId_; }
    QVariantMap selection() const;
    QString hint() const;
    QString loadTitle() const;
    int seatsFilled() const;
    int seatsTotal() const;
    int seatSegments() const { return std::min(seatsTotal(), 60); }
    int seatSegmentsFilled() const;
    QString loadText() const;
    QVariantList weightBars() const;
    QString planTitle() const;
    QString originIcao() const;
    QString destinationIcao() const;
    QString departureText() const;
    QString arrivalText() const;
    QVariantList planCells() const;
    QString routeForCharts() const;
    QString openSimbriefLabel() const;
    QString importLabel() const;
    bool busy() const { return busy_; }
    QString message() const { return message_; }

    // The report's money: the stored breakdown once accepted, else priced now (estimate before import, no fuel).
    std::optional<blocktime::Preview> preview() const;

signals:
    void changed();
    void dispatched();  // a planned flight was written: Fleet's cards refresh

private:
    const std::optional<blocktime::PlannedFlightView>& view() const { return yourFlight_.view(); }
    void onBlockEvent(const QString& type);
    void reloadRates();
    blocktime::ChangeFacts factsOf(const blocktime::PlannedFlightView& v) const;
    void setMessage(QString message);
    void openForm(blocktime::FlightDraft draft, bool changing);

    blocktime::db::Database& db_;
    QString airlineIcao_;
    std::filesystem::path ratesFile_;
    Planner& planner_;
    RecorderBridge& bridge_;
    blocktime::AppEventQueue& appEvents_;
    YourFlightModel& yourFlight_;
    blocktime::Rates rates_;
    bool ratesOk_ = false;
    QString ratesError_;  // why rates.yaml didn't read; "" when it did

    std::optional<blocktime::FlightDraft> draft_;
    bool changing_ = false;
    QString destination_, additionalFuel_;

    // The report before the last re-import, for what changed (D-251: in memory only).
    std::optional<blocktime::Preview> before_;
    blocktime::ChangeFacts beforeFacts_;
    std::int64_t beforeFlightId_ = 0;
    std::set<std::string> changedIds_;
    int importCount_ = 0;
    QString selectedId_;
    bool busy_ = false;
    QString message_;
    int request_ = 0;  // bumped by each open, dispatch and cancel: an answer for an older request is dropped
};
