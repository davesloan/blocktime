// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fleet_table_model.h"

#include <format>

#include "leasing.h"

#include <algorithm>
#include <utility>

namespace {

// "12,128": thousands separators, the same in every locale.
QString withCommas(int value) {
    QString digits = QString::number(value < 0 ? -value : value);
    for (qsizetype i = digits.size() - 3; i > 0; i -= 3) digits.insert(i, u',');
    return value < 0 ? u'-' + digits : digits;
}

QString statusOf(const blocktime::AirplaneView& a) {
    if (a.returnedOn) return QStringLiteral("Returned ") + QString::fromStdString(blocktime::formatDayMonthYear(*a.returnedOn));
    if (a.awaitingDelivery) return QStringLiteral("Awaiting delivery");
    return QStringLiteral("Parked");
}

QString textOf(const blocktime::AirplaneView& a, int column) {
    switch (column) {
    case FleetTableModel::Registration: return QString::fromStdString(a.registration);
    case FleetTableModel::Type: return QString::fromStdString(a.typeCode);
    case FleetTableModel::Airframe: return QString::fromStdString(a.airframeName);
    case FleetTableModel::Location: return QString::fromStdString(a.locationIcao);
    case FleetTableModel::Fuel: return withCommas(a.fuelOnBoardLb) + QStringLiteral(" lb");
    case FleetTableModel::Flights: return QString::number(a.flights);
    case FleetTableModel::Hours: return QString::number(a.blockHours, 'f', 1);
    case FleetTableModel::Status: return statusOf(a);
    default: return {};
    }
}

}  // namespace

FleetTableModel::FleetTableModel(std::vector<blocktime::AirplaneView> airplanes, QStringList notices, QString summaryText,
                                 QObject* parent)
    : QAbstractTableModel(parent), rows_(std::move(airplanes)), notices_(std::move(notices)),
      summaryText_(std::move(summaryText)) {
    applySort();
}

void FleetTableModel::reset(std::vector<blocktime::AirplaneView> airplanes, QStringList notices, QString summaryText) {
    if (summaryText != summaryText_) {
        summaryText_ = std::move(summaryText);
        emit summaryTextChanged();
    }
    beginResetModel();
    rows_ = std::move(airplanes);
    applySort();
    endResetModel();
    if (notices != notices_) {
        notices_ = std::move(notices);
        emit noticesChanged();
    }
}

int FleetTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int FleetTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : ColumnCount; }

QVariant FleetTableModel::data(const QModelIndex& index, int role) const {
    if (role != Qt::DisplayRole || !index.isValid() || index.row() >= rowCount()) return {};
    return textOf(rows_[static_cast<std::size_t>(index.row())], index.column());
}

QString FleetTableModel::fleetSummary(double hoursFlown, int inFleet) {
    const int earned = blocktime::slotsEarned(hoursFlown);
    return QString::fromStdString(std::format("{:.1f} h flown · {} of {} slots in use · slot {} at {:.0f} h", hoursFlown,
                                              inFleet, earned, earned + 1, blocktime::hoursForSlot(earned + 1)));
}

QVariant FleetTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};
    static const char* const titles[ColumnCount] = {"Registration", "Type",  "Airframe", "Location",
                                                    "Fuel",         "Flights", "Hours", "Status"};
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

void FleetTableModel::applySort() {
    const int column = sortColumn_;
    const bool descending = sortOrder_ == Qt::DescendingOrder;
    std::stable_sort(rows_.begin(), rows_.end(), [column, descending](const auto& a, const auto& b) {
        int c = 0;
        if (column == Fuel) c = (a.fuelOnBoardLb > b.fuelOnBoardLb) - (a.fuelOnBoardLb < b.fuelOnBoardLb);
        else if (column == Flights) c = (a.flights > b.flights) - (a.flights < b.flights);
        else if (column == Hours) c = (a.blockHours > b.blockHours) - (a.blockHours < b.blockHours);
        else c = QString::compare(textOf(a, column), textOf(b, column), Qt::CaseInsensitive);
        if (c == 0) c = a.registration.compare(b.registration);  // ties: by registration, ascending
        else if (descending) c = -c;
        return c < 0;
    });
}

QString FleetTableModel::registrationAt(int row) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) return {};
    return QString::fromStdString(rows_[static_cast<std::size_t>(row)].registration);
}
