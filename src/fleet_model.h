// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Fleet screen (D-241, docs/fleet.md): the airline as a character sheet (slots open, the next slot, hours, cash,
// leases) and a card per airplane in the fleet, in lease order, with its main button. Rotation and journey cards come
// with those milestones; the `group` role is always "" until then.

#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "airline_queries.h"
#include "money.h"

// Everything the Fleet screen shows, from the airline session and the planned flight.
struct FleetState {
    std::vector<blocktime::AirplaneView> airplanes;  // returned ones are left out of the cards
    QStringList notices;
    double hoursFlown = 0;
    blocktime::Cents cashCents = 0;
    QString airlineName, airlineCode, callsign;
    QString plannedRegistration;  // the airplane with the planned or in-progress flight; "" for none
    QString plannedLeg;           // "XLY 12 · KSAN → KLAX"
};

class FleetModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(QString airlineName READ airlineName NOTIFY changed)
    Q_PROPERTY(QString airlineCode READ airlineCode NOTIFY changed)
    Q_PROPERTY(QString callsign READ callsign NOTIFY changed)
    Q_PROPERTY(int slotsOpen READ slotsOpen NOTIFY changed)
    Q_PROPERTY(int slotsMax READ slotsMax CONSTANT)
    Q_PROPERTY(int emptySlots READ emptySlots NOTIFY changed)
    Q_PROPERTY(double nextSlotHours READ nextSlotHours NOTIFY changed)       // 0 once all 20 are open
    Q_PROPERTY(double nextSlotProgress READ nextSlotProgress NOTIFY changed)  // 0..1 through the current 10 h
    Q_PROPERTY(QString nextSlotText READ nextSlotText NOTIFY changed)        // "23.4 / 30 h"
    Q_PROPERTY(QVariantList lockedSlotHours READ lockedSlotHours NOTIFY changed)
    Q_PROPERTY(QString hoursText READ hoursText NOTIFY changed)
    Q_PROPERTY(QString cashText READ cashText NOTIFY changed)
    Q_PROPERTY(QString leasesText READ leasesText NOTIFY changed)
    Q_PROPERTY(QStringList notices READ notices NOTIFY changed)

public:
    enum Role { Registration = Qt::UserRole + 1, Title, Place, Leg, Glyph, Button, ButtonEnabled, Group };

    explicit FleetModel(FleetState state, QObject* parent = nullptr);

    void reset(FleetState state);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString registrationAt(int row) const;  // "" for a row that doesn't exist

    QString airlineName() const { return state_.airlineName; }
    QString airlineCode() const { return state_.airlineCode; }
    QString callsign() const { return state_.callsign; }
    int slotsOpen() const;
    int slotsMax() const;
    int emptySlots() const;
    double nextSlotHours() const;
    double nextSlotProgress() const;
    QString nextSlotText() const;
    QVariantList lockedSlotHours() const;
    QString hoursText() const;
    QString cashText() const;
    QString leasesText() const;
    QStringList notices() const { return state_.notices; }
    QString plannedRegistration() const { return state_.plannedRegistration; }
    QString plannedLeg() const { return state_.plannedLeg; }

signals:
    void changed();

private:
    FleetState state_;
    std::vector<blocktime::AirplaneView> cards_;  // in the fleet, by airplane id (lease order)
};
