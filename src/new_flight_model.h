// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The New Flight dialog (design.md §13.1): a FlightDraft shown to QML, and Plan flight, which fetches the ATIS
// runways, writes the planned flight and opens SimBrief's dispatch page (§13.2).

#pragma once

#include <filesystem>
#include <optional>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "flight_draft.h"
#include "money.h"
#include "planner.h"
#include "sqlite_db.h"

class NewFlightModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(QString registration READ registration NOTIFY changed)
    Q_PROPERTY(QString airplaneText READ airplaneText NOTIFY changed)
    Q_PROPERTY(QString origin READ origin NOTIFY changed)
    Q_PROPERTY(QStringList kinds READ kinds NOTIFY changed)
    Q_PROPERTY(QStringList kindLabels READ kindLabels NOTIFY changed)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY changed)
    Q_PROPERTY(bool bellyCargoAvailable READ bellyCargoAvailable NOTIFY changed)
    Q_PROPERTY(bool bellyCargo READ bellyCargo WRITE setBellyCargo NOTIFY changed)
    Q_PROPERTY(QString destination READ destination WRITE setDestination NOTIFY changed)
    Q_PROPERTY(QString destinationMessage READ destinationMessage NOTIFY changed)
    Q_PROPERTY(QString flightNumber READ flightNumber WRITE setFlightNumber NOTIFY changed)
    Q_PROPERTY(QStringList loadLines READ loadLines NOTIFY changed)
    Q_PROPERTY(QVariantList incomeLines READ incomeLines NOTIFY changed)
    Q_PROPERTY(QVariantList costLines READ costLines NOTIFY changed)
    Q_PROPERTY(QString totalText READ totalText NOTIFY changed)
    Q_PROPERTY(QString fuelNote READ fuelNote NOTIFY changed)
    Q_PROPERTY(QString endsUpText READ endsUpText NOTIFY changed)
    Q_PROPERTY(bool canPlan READ canPlan NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    // rates.yaml is read at every open() (D-107).
    NewFlightModel(blocktime::db::Database& db, QString airlineIcao, std::filesystem::path ratesFile, Planner& planner,
                   QObject* parent = nullptr);

    // Starts a new draft for the airplane: rolls its loads now. False if it can't fly (unknown or returned).
    Q_INVOKABLE bool open(const QString& registration);
    // Fetches the runways, writes the planned flight and opens the dispatch page; emits planned() when done. The
    // flight is taken as it is at the click; cancel() or open() before the runways arrive drops it.
    Q_INVOKABLE void plan();
    Q_INVOKABLE void cancel();

    QString registration() const;
    QString airplaneText() const;
    QString origin() const;
    QStringList kinds() const;
    QStringList kindLabels() const;
    QString kind() const;
    void setKind(const QString& code);
    bool bellyCargoAvailable() const;
    bool bellyCargo() const;
    void setBellyCargo(bool on);
    QString destination() const { return destination_; }
    void setDestination(const QString& typed);
    QString destinationMessage() const;
    QString flightNumber() const;
    void setFlightNumber(const QString& number);
    QStringList loadLines() const;
    QVariantList incomeLines() const;
    QVariantList costLines() const;
    QString totalText() const;
    QString fuelNote() const;
    QString endsUpText() const;
    bool canPlan() const;
    bool busy() const { return busy_; }
    QString error() const { return error_; }

    // A kind's label for the dialog: "VIP airliner", "Charter", "Passengers", "Cargo", "Delivery flight".
    static QString kindLabel(blocktime::FlightKind kind);

signals:
    void changed();
    void planned(QString note);  // note: why there are no ATIS runways, or ""

private:
    blocktime::db::Database& db_;
    QString airlineIcao_;
    std::filesystem::path ratesFile_;
    bool ratesOk_ = false;  // rates.yaml read at the last open(); Plan flight needs it
    Planner& planner_;
    std::optional<blocktime::FlightDraft> draft_;
    QString destination_;
    bool busy_ = false;
    QString error_;
    int request_ = 0;  // bumped by open(), cancel() and plan(): an answer for an older request is dropped
};
