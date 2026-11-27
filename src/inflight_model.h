// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The in-flight panel's data (D-194..D-198, D-203), from block out to block in, on Live and in the replay window:
// the lamps, the gauges, the ATC card and the phase's readouts. The logic is in plain C++ (lamps.h, atc_card.h,
// inflight_readouts.h, altitude_face.h); this turns it into QML properties.

#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <nlohmann/json.hpp>

#include "altitude_face.h"
#include "atc_card.h"
#include "lamps.h"
#include "planned_flight_display.h"
#include "preflight_log.h"
#include "recorder_bridge.h"

class InFlightModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")

    Q_PROPERTY(bool visible READ visible NOTIFY changed)            // the flight is open: block out to block in
    Q_PROPERTY(bool animate READ animate NOTIFY animateChanged)     // false while a replay catches up: nothing flips
    // [{name: "BEACON", lens: "red"|"white"|"amber"|"green", known, lit, wrong}] in the strip's order
    Q_PROPERTY(QVariantList lamps READ lamps NOTIFY changed)
    Q_PROPERTY(bool flashOn READ flashOn NOTIFY flashChanged)       // toggles every second while a lamp is wrong
    Q_PROPERTY(QString speedFace READ speedFace NOTIFY changed)     // "gs" on the ground below 40 kt, else "ias"
    Q_PROPERTY(double speedKt READ speedKt NOTIFY changed)
    Q_PROPERTY(QString machText READ machText NOTIFY changed)       // ".78"; "" low down
    Q_PROPERTY(double vrefBugKt READ vrefBugKt NOTIFY changed)      // -1: no bug (only from Approach to touchdown)
    Q_PROPERTY(double altitudeFt READ altitudeFt NOTIFY changed)
    Q_PROPERTY(QString altimeterSetting READ altimeterSetting NOTIFY changed)  // "STD", "30.09", "1013"; ""
    Q_PROPERTY(QString altitudeFace READ altitudeFace NOTIFY changed)          // "altimeter" or "radio" (D-203)
    Q_PROPERTY(double aglFt READ aglFt NOTIFY changed)
    Q_PROPERTY(double vsFpm READ vsFpm NOTIFY changed)
    Q_PROPERTY(double fuelLb READ fuelLb NOTIFY changed)
    Q_PROPERTY(double fuelScaleLb READ fuelScaleLb NOTIFY changed)  // the block-out fuel rounded up to 6,000 lb
    Q_PROPERTY(double lowFuelLb READ lowFuelLb NOTIFY changed)      // the plan's final reserve; 0 = no arc
    Q_PROPERTY(QString fuelText READ fuelText NOTIFY changed)       // "7,410": the gauge's window
    // [{title, value, aside, latest, spacer}]: aside is the local time when the slot has no qualifier
    Q_PROPERTY(QVariantList atcSlots READ atcSlots NOTIFY changed)
    Q_PROPERTY(QString phaseTitle READ phaseTitle NOTIFY changed)   // "DEPARTURE", "CLIMB", … "LANDED"
    // [{title, value, unit, tag, flagged, words, wide, ticks, stars}]
    Q_PROPERTY(QVariantList readouts READ readouts NOTIFY changed)

public:
    // vipAttendantsTimes: rates.yaml's VIP flight attendant multiple, for SOULS.
    InFlightModel(RecorderBridge& bridge, PlannedFlightDisplay& planned, int vipAttendantsTimes,
                  QObject* parent = nullptr);

    // A burst of lines (a replay seek or restart) goes between these: animate is off from the first, and comes back
    // on only after everything the burst queued (the cards' Qt.callLater checks) has run, so nothing flips (D-198).
    void beginCatchUp();
    void endCatchUp();
    void setFlashInterval(std::chrono::milliseconds interval);  // 1 s in the app (D-195); tests use less

    bool visible() const;
    bool animate() const { return animate_; }
    QVariantList lamps() const { return lamps_; }
    bool flashOn() const { return flashOn_; }
    QString speedFace() const;
    double speedKt() const;
    QString machText() const;
    double vrefBugKt() const;
    double altitudeFt() const { return bridge_.altitudeFt(); }
    QString altimeterSetting() const;
    QString altitudeFace() const { return face_.radio() ? QStringLiteral("radio") : QStringLiteral("altimeter"); }
    double aglFt() const { return bridge_.lastSample().altitudeAglFt; }
    double vsFpm() const { return bridge_.vsFpm(); }
    double fuelLb() const { return bridge_.fuelLb(); }
    double fuelScaleLb() const;
    double lowFuelLb() const;
    QString fuelText() const;
    QVariantList atcSlots() const { return atcSlots_; }
    QString phaseTitle() const { return phaseTitle_; }
    QVariantList readouts() const { return readouts_; }

signals:
    void changed();
    void animateChanged();
    void flashChanged();

private:
    void refreshFlight();
    void onFlightEvent(const QString& type, const QVariantMap& data);
    void onBlockEvent(const QString& type);
    void onSample();
    void rebuild();  // the lists from the parts, then `changed`

    RecorderBridge& bridge_;
    PlannedFlightDisplay& planned_;
    int vipAttendantsTimes_;
    blocktime::PlannedFlightView flight_;
    bool hasFlight_ = false;
    blocktime::LampBoard board_;
    blocktime::AtcCard card_;
    blocktime::PreFlightLog log_;  // the origin's clearance and ATIS, for the Departure readouts
    blocktime::AltitudeFace face_;
    std::string phase_ = "at_gate";
    bool onFinal_ = false, touchedDown_ = false;
    std::optional<nlohmann::json> cockpit_, altimeter_, destinationAtis_, latestAtis_, touchdown_;
    std::optional<int> stars_;
    std::string gate_;
    QTimer flash_;
    bool flashOn_ = true, animate_ = false, catchingUp_ = false;
    QVariantList lamps_, atcSlots_, readouts_;
    QString phaseTitle_;
};
