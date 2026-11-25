// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The event feed shown beside the flight panel: newest first, capped so it can't grow during a long session.

#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

class EventFeedModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by RecorderBridge")

public:
    enum Role { TimeRole = Qt::UserRole + 1, KindRole, TextRole };
    static constexpr int kMaxRows = 200;

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Adds a row at the top, stamped with `at` in UTC (HH:mm:ss): the bridge's clock (D-166). kind: "block", "warn" or
    // "info".
    void add(const QString& kind, const QString& text, const QDateTime& at);

private:
    struct Row {
        QString time, kind, text;
    };
    QList<Row> rows_;
};
