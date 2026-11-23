// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Flights screen's list (D-111): the settled flights from the airline database, newest first, each with its money
// for the summary. Tracks still come from the event log (D-060), read on a worker thread and joined on the flight id;
// a flight whose log is gone shows a straight line between its airports.

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <QAbstractListModel>
#include <QGeoPath>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "airline_queries.h"
#include "flight_history.h"
#include "sqlite_db.h"

class FlightListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)  // the tracks are being read

public:
    enum Role {
        FlightIdRole = Qt::UserRole + 1,
        DateTextRole,      // "22 Nov 2026" (UTC, block out)
        OutTextRole,       // "10:30Z"
        InTextRole,        // "13:20Z"
        RouteTextRole,     // "KBFI–KSAN", or "KBFI–KSAN, diverted to KSHT"
        FlightNumberRole,  // "XLY8"
        RegistrationRole,
        TypeRole,
        BlockTextRole,     // "2:50"
        FuelUsedLbRole,    // fuel out - fuel in
        TotalTextRole,     // short form: "-$13.1k"
        TotalCentsRole,
        DistanceNmRole,    // flown, from the log's track; -1 when the log no longer has it
        MaxAltFtRole,      // the same
    };

    // Reads nothing yet: call reload().
    FlightListModel(blocktime::db::Database& db, std::filesystem::path logDir, QString airlineIcao,
                    QObject* parent = nullptr);
    // Stops a track load that's still running and waits for it (it returns at once when stopped).
    ~FlightListModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(flights_.size()); }
    bool loading() const { return loading_; }

    // Reads the flights from the database now (it's quick), then the tracks from the log on a worker thread, since 30
    // days of long flights take seconds to read. A reload asked for while tracks are loading runs again when they end.
    Q_INVOKABLE void reload();
    // The same, tracks included, on the calling thread, for tests.
    void reloadBlocking();
    // The flight's track for the map: the logged one, or its two airports. Empty for a bad row.
    Q_INVOKABLE QGeoPath track(int row) const;
    // What FlightSummary shows: { title, dateText, outText, inText, blockText, fuelText, routeText,
    // income: [{label, amount}], costs: [...], totalText }. Empty for a bad row.
    Q_INVOKABLE QVariantMap summary(int row) const;
    Q_INVOKABLE int rowOf(const QString& flightId) const;  // -1 if it isn't listed

signals:
    void countChanged();
    void loadingChanged();
    void tracksChanged();

private:
    void readFlights();
    void applyTracks(std::vector<blocktime::FlightRecord> records);
    void setLoading(bool loading);
    const blocktime::FlightRecord* recordOf(const blocktime::FlightView& f) const;

    blocktime::db::Database& db_;
    std::filesystem::path logDir_;
    QString airlineIcao_;
    std::vector<blocktime::FlightView> flights_;
    std::map<std::string, blocktime::FlightRecord> tracks_;  // by flight id
    bool loading_ = false;
    bool reloadAgain_ = false;
    std::jthread worker_;  // last, so it's stopped and joined before the members it uses go
};
