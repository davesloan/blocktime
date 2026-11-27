// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "money_lines_qml.h"

#include <QString>
#include <QVariantMap>

#include "money.h"

QVariantList moneyLinesToQml(const std::vector<blocktime::MoneyLine>& lines) {
    QVariantList out;
    for (const blocktime::MoneyLine& m : lines) {
        out << QVariantMap{{"category", QString::fromStdString(m.category)},
                           {"label", QString::fromStdString(m.label)},
                           {"amount", QString::fromStdString(blocktime::formatMoneyFull(m.cents))},
                           {"working", QString::fromStdString(blocktime::workingText(m))}};
    }
    return out;
}
