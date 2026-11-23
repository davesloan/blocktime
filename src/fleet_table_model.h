// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Fleet screen's table: the airline's airplanes from the database (AirlineSession), sortable by any column, plus
// the session's notices shown above it. Flight counts and hours come back in milestone 6 as stored totals (D-097).

#pragma once

#include <vector>

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include "airline_queries.h"

class FleetTableModel : public QAbstractTableModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(int sortOrder READ sortOrder NOTIFY sortChanged)  // Qt::SortOrder: 0 ascending, 1 descending
    Q_PROPERTY(QStringList notices READ notices NOTIFY noticesChanged)

public:
    enum Column { Registration, Type, Airframe, Fleet, Location, Fuel, Status, ColumnCount };

    FleetTableModel(std::vector<blocktime::AirplaneView> airplanes, QStringList notices, QObject* parent = nullptr);

    // Replaces the rows and notices, keeping the current sort.
    void reset(std::vector<blocktime::AirplaneView> airplanes, QStringList notices);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Text columns compare case-insensitively, Fuel by value; ties fall back to the registration.
    Q_INVOKABLE void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // The registration shown in `row`, or "" for a row that doesn't exist.
    Q_INVOKABLE QString registrationAt(int row) const;

    int sortColumn() const { return sortColumn_; }
    int sortOrder() const { return static_cast<int>(sortOrder_); }
    QStringList notices() const { return notices_; }

signals:
    void sortChanged();
    void noticesChanged();

private:
    void applySort();

    std::vector<blocktime::AirplaneView> rows_;
    QStringList notices_;
    int sortColumn_ = Registration;
    Qt::SortOrder sortOrder_ = Qt::AscendingOrder;
};
