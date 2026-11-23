// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Money screen: the ledger lines, newest first, and the cash in its short and full forms (D-080).

#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "airline_queries.h"
#include "money.h"

class LedgerModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(QString cashShort READ cashShort NOTIFY changed)
    Q_PROPERTY(QString cashFull READ cashFull NOTIFY changed)

public:
    enum Role { DateRole = Qt::UserRole + 1, TextRole, AmountRole, IncomeRole };

    LedgerModel(std::vector<blocktime::LedgerView> lines, blocktime::Cents cash, QObject* parent = nullptr);

    void reset(std::vector<blocktime::LedgerView> lines, blocktime::Cents cash);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString cashShort() const;
    QString cashFull() const;

signals:
    void changed();

private:
    std::vector<blocktime::LedgerView> lines_;
    blocktime::Cents cash_ = 0;
};
