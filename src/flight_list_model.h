// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Flights screen's list (D-060): a thin Qt model over loadFlightHistory. All the reading and track logic is in
// flight_history.cpp; this only formats it for QML.

#pragma once

#include <filesystem>
#include <thread>
#include <vector>

#include <QAbstractListModel>
#include <QGeoPath>
#include <QtQml/qqmlregistration.h>

#include "flight_history.h"

class FlightListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    enum Role {
        FlightIdRole = Qt::UserRole + 1,
        DateTextRole,    // "22 Nov 2026" (UTC)
        OutTextRole,     // "21:28Z"
        InTextRole,      // "21:55Z", or "" if the flight has no end
        AtcIdRole,
        TitleRole,
        BlockTextRole,   // "0:26"; from block_time_s, else out → end, else "—"
        FuelUsedLbRole,  // out − in, or -1 without a block in
        DistanceNmRole,
        MaxAltFtRole,
        StatusRole,      // "block_in", "interrupted" or "open"
        StatusTextRole,  // "Block in", "Interrupted · sim_stopped", "In progress"
        EndReasonRole,
    };

    // Doesn't read the log yet: call reload().
    explicit FlightListModel(std::filesystem::path logDir, QObject* parent = nullptr);
    // Stops a load that's still running and waits for it (it returns at once when stopped).
    ~FlightListModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(flights_.size()); }
    bool loading() const { return loading_; }

    // Reads the log again on a worker thread and swaps the result in on the GUI thread with a model reset. 30 days of
    // long flights take seconds to read, which must not freeze the window. A reload asked for while one is running
    // runs again when it ends, so the newest block event is always included.
    Q_INVOKABLE void reload();
    // The same on the calling thread, for tests.
    void reloadBlocking();
    // The thinned track of the flight in `row`, for the map. Empty for a bad row.
    Q_INVOKABLE QGeoPath track(int row) const;

    const std::vector<blocktime::FlightRecord>& records() const { return flights_; }

signals:
    void countChanged();
    void loadingChanged();

private:
    void apply(std::vector<blocktime::FlightRecord> flights);
    void setLoading(bool loading);

    std::filesystem::path logDir_;
    std::vector<blocktime::FlightRecord> flights_;
    bool loading_ = false;
    bool reloadAgain_ = false;
    std::jthread worker_;  // last, so it's stopped and joined before the members it uses go
};
