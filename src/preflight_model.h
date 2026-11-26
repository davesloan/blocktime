// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the Pre-Flight panel shows (docs/dashboard.md, "Pre-Flight panel"; D-153, D-187, D-188, D-189): the load
// sheet, the departure conditions and the clearance, as texts ready for QML. It feeds the bridge's events to a
// PreFlightLog and reads the planned flight on show. Unknown values are "": QML draws a dim dash. Live and the replay
// window each get one. The countdown has its own signal, so its once-a-second tick doesn't rebuild the rest.

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "load_sheet.h"
#include "planned_flight_display.h"
#include "preflight_log.h"
#include "recorder_bridge.h"

class PreFlightModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")

    // A planned flight on show, until block out (D-194).
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(bool hasPlan READ hasPlan NOTIFY changed)
    // The load sheet: "13,439 lb"; passengers and holds as [{label, value}] (one cabin is just "Passengers").
    Q_PROPERTY(QString blockFuelText READ blockFuelText NOTIFY changed)
    Q_PROPERTY(QString zfwText READ zfwText NOTIFY changed)
    Q_PROPERTY(QVariantList passengers READ passengers NOTIFY changed)
    Q_PROPERTY(QString cargoText READ cargoText NOTIFY changed)
    Q_PROPERTY(QVariantList holds READ holds NOTIFY changed)
    Q_PROPERTY(QString cruiseText READ cruiseText NOTIFY changed)          // "FL400" (D-187)
    Q_PROPERTY(QString transitionText READ transitionText NOTIFY changed)  // "18,000"
    // Departure: the runway with where it came from ("PLAN", "ATIS", "ATC", or "PLAN 27" when it changed).
    Q_PROPERTY(QString runway READ runway NOTIFY changed)
    Q_PROPERTY(bool runwayChanged READ runwayChanged NOTIFY changed)
    Q_PROPERTY(QString runwayTag READ runwayTag NOTIFY changed)
    Q_PROPERTY(QString headingText READ headingText NOTIFY changed)  // "275°"
    Q_PROPERTY(QString conditionText READ conditionText NOTIFY changed)
    Q_PROPERTY(QString windText READ windText NOTIFY changed)
    Q_PROPERTY(QString temperatureText READ temperatureText NOTIFY changed)
    Q_PROPERTY(QString altimeterText READ altimeterText NOTIFY changed)
    Q_PROPERTY(QString atisLetter READ atisLetter NOTIFY changed)
    // The clearance: the SID is SimBrief's until ATC changes it (D-189); its tag "PLAN", "CLEARED" or "PLAN PADRZ2".
    Q_PROPERTY(bool cleared READ cleared NOTIFY changed)
    Q_PROPERTY(QString sid READ sid NOTIFY changed)
    Q_PROPERTY(bool sidChanged READ sidChanged NOTIFY changed)
    Q_PROPERTY(QString sidTag READ sidTag NOTIFY changed)
    Q_PROPERTY(QString initialAltitude READ initialAltitude NOTIFY changed)
    Q_PROPERTY(QString squawk READ squawk NOTIFY changed)
    // The 30-minute countdown from the clearance until block out: "26:40", red in the last 5 minutes.
    Q_PROPERTY(bool countdownVisible READ countdownVisible NOTIFY countdownChanged)
    Q_PROPERTY(QString countdownText READ countdownText NOTIFY countdownChanged)
    Q_PROPERTY(bool countdownUrgent READ countdownUrgent NOTIFY countdownChanged)

public:
    PreFlightModel(RecorderBridge& bridge, PlannedFlightDisplay& planned, QObject* parent = nullptr);

    bool visible() const;
    bool hasPlan() const { return sheet_.hasPlan; }
    QString blockFuelText() const;
    QString zfwText() const;
    QVariantList passengers() const;
    QString cargoText() const;
    QVariantList holds() const;
    QString cruiseText() const;
    QString transitionText() const;
    QString runway() const { return q(departure_.runway); }
    bool runwayChanged() const { return departure_.runwayChanged; }
    QString runwayTag() const;
    QString headingText() const;
    QString conditionText() const { return q(departure_.condition); }
    QString windText() const { return q(departure_.wind); }
    QString temperatureText() const { return q(departure_.temperature); }
    QString altimeterText() const { return q(departure_.altimeter); }
    QString atisLetter() const { return q(departure_.atisLetter); }
    bool cleared() const { return clearance_.received; }
    QString sid() const { return q(clearance_.sid); }
    bool sidChanged() const { return clearance_.sidChanged; }
    QString sidTag() const;
    QString initialAltitude() const { return q(clearance_.initialAltitude); }
    QString squawk() const { return q(clearance_.squawk); }
    bool countdownVisible() const;
    QString countdownText() const;
    bool countdownUrgent() const;

signals:
    void changed();
    void countdownChanged();

private:
    static QString q(const std::string& s) { return QString::fromStdString(s); }
    void refreshFlight();
    void rebuild();  // the views from the flight and the log, then the signals
    void updateTimer();

    RecorderBridge& bridge_;
    PlannedFlightDisplay& planned_;
    blocktime::PreFlightLog log_;
    blocktime::PlannedFlightView flight_;  // the last flight on show
    bool hasFlight_ = false;
    blocktime::LoadSheet sheet_;
    blocktime::DepartureView departure_;
    blocktime::ClearanceView clearance_;
    QTimer timer_;  // once a second, only while the countdown shows
};
