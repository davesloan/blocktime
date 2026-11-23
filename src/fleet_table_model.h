// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Fleet screen's table (D-061): the airframes from FleetStore, plus each one's flights and block hours counted
// from the flight history (block-in flights whose atc_id is the registration). Sortable by any column.

#pragma once

#include <vector>

#include <QAbstractTableModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "fleet_store.h"

class FlightListModel;

class FleetTableModel : public QAbstractTableModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(int sortOrder READ sortOrder NOTIFY sortChanged)  // Qt::SortOrder: 0 ascending, 1 descending
    Q_PROPERTY(QString error READ error CONSTANT)                // "" when the store opened

public:
    enum Column { Registration, Type, Configuration, Location, Status, Flights, BlockHours, ColumnCount };

    // `error` is the store's open error ("" if it opened). Recounts whenever `flights` is reloaded.
    FleetTableModel(std::vector<blocktime::Airframe> fleet, QString error, const FlightListModel& flights,
                    QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Text columns compare case-insensitively, the number columns by value; ties fall back to the registration.
    Q_INVOKABLE void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    int sortColumn() const { return sortColumn_; }
    int sortOrder() const { return static_cast<int>(sortOrder_); }
    QString error() const { return error_; }

signals:
    void sortChanged();

private:
    struct Row {
        blocktime::Airframe airframe;
        int flights = 0;
        double blockHours = 0;
    };

    void recount();
    void applySort();

    std::vector<Row> rows_;
    QString error_;
    const FlightListModel& flights_;
    int sortColumn_ = Registration;
    Qt::SortOrder sortOrder_ = Qt::AscendingOrder;
};
