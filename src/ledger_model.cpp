// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ledger_model.h"

#include <utility>

#include "dates.h"

LedgerModel::LedgerModel(std::vector<blocktime::LedgerView> lines, blocktime::Cents cash, QObject* parent)
    : QAbstractListModel(parent), lines_(std::move(lines)), cash_(cash) {}

void LedgerModel::reset(std::vector<blocktime::LedgerView> lines, blocktime::Cents cash) {
    beginResetModel();
    lines_ = std::move(lines);
    cash_ = cash;
    endResetModel();
    emit changed();
}

int LedgerModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(lines_.size());
}

QVariant LedgerModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const blocktime::LedgerView& line = lines_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case DateRole: {
        // posted_at is an instant whose shape the schema checks; its first 10 characters are the date.
        const auto date = blocktime::parseDate(std::string_view(line.postedAt).substr(0, 10));
        return date ? QString::fromStdString(blocktime::formatDayMonthYear(*date)) : QString();
    }
    case TextRole: return QString::fromStdString(line.text);
    case AmountRole: return QString::fromStdString(blocktime::formatMoneyFull(line.amountCents));
    case IncomeRole: return line.amountCents > 0;
    default: return {};
    }
}

QHash<int, QByteArray> LedgerModel::roleNames() const {
    return {{DateRole, "date"}, {TextRole, "text"}, {AmountRole, "amount"}, {IncomeRole, "income"}};
}

QString LedgerModel::cashShort() const { return QString::fromStdString(blocktime::formatMoneyShort(cash_)); }

QString LedgerModel::cashFull() const { return QString::fromStdString(blocktime::formatMoneyFull(cash_)); }
