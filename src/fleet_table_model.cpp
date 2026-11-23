// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fleet_table_model.h"

#include <algorithm>
#include <utility>

#include "flight_list_model.h"

namespace {

QString textOf(const blocktime::Airframe& a, int column) {
    switch (column) {
    case FleetTableModel::Registration: return QString::fromStdString(a.registration);
    case FleetTableModel::Type: return QString::fromStdString(a.type);
    case FleetTableModel::Configuration: return QString::fromStdString(a.configuration);
    case FleetTableModel::Location: return QString::fromStdString(a.location);
    case FleetTableModel::Status: return QString::fromStdString(a.status);
    default: return {};
    }
}

}  // namespace

FleetTableModel::FleetTableModel(std::vector<blocktime::Airframe> fleet, QString error,
                                 const FlightListModel& flights, QObject* parent)
    : QAbstractTableModel(parent), error_(std::move(error)), flights_(flights) {
    rows_.reserve(fleet.size());
    for (auto& a : fleet) rows_.push_back(Row{std::move(a), 0, 0});
    recount();
    applySort();
    connect(&flights_, &QAbstractItemModel::modelReset, this, [this] {
        beginResetModel();
        recount();
        applySort();
        endResetModel();
    });
}

int FleetTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int FleetTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : ColumnCount; }

QVariant FleetTableModel::data(const QModelIndex& index, int role) const {
    if (role != Qt::DisplayRole || !index.isValid() || index.row() >= rowCount()) return {};
    const Row& r = rows_[static_cast<std::size_t>(index.row())];
    switch (index.column()) {
    case Flights: return QString::number(r.flights);
    case BlockHours: return QString::number(r.blockHours, 'f', 1);
    default: return textOf(r.airframe, index.column());
    }
}

QVariant FleetTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};
    static const char* const titles[ColumnCount] = {"Registration", "Type",    "Configuration", "Location",
                                                    "Status",       "Flights", "Block hours"};
    return section >= 0 && section < ColumnCount ? QString::fromLatin1(titles[section]) : QVariant();
}

void FleetTableModel::sort(int column, Qt::SortOrder order) {
    if (column < 0 || column >= ColumnCount) return;
    beginResetModel();
    sortColumn_ = column;
    sortOrder_ = order;
    applySort();
    endResetModel();
    emit sortChanged();
}

void FleetTableModel::recount() {
    for (auto& r : rows_) {
        r.flights = 0;
        r.blockHours = 0;
        for (const auto& f : flights_.records()) {
            if (f.end != blocktime::FlightEnd::BlockIn || f.atcId != r.airframe.registration) continue;
            ++r.flights;
            r.blockHours += f.blockTimeS.value_or(0) / 3600.0;
        }
    }
}

void FleetTableModel::applySort() {
    const int column = sortColumn_;
    const bool descending = sortOrder_ == Qt::DescendingOrder;
    std::stable_sort(rows_.begin(), rows_.end(), [column, descending](const Row& a, const Row& b) {
        int c = 0;
        if (column == Flights) c = (a.flights > b.flights) - (a.flights < b.flights);
        else if (column == BlockHours) c = (a.blockHours > b.blockHours) - (a.blockHours < b.blockHours);
        else c = QString::compare(textOf(a.airframe, column), textOf(b.airframe, column), Qt::CaseInsensitive);
        if (c == 0) c = a.airframe.registration.compare(b.airframe.registration);  // ties: by registration, ascending
        else if (descending) c = -c;
        return c < 0;
    });
}
