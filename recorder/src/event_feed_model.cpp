// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "event_feed_model.h"

#include <QDateTime>

int EventFeedModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant EventFeedModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
    const Row& r = rows_.at(index.row());
    switch (role) {
        case TimeRole: return r.time;
        case KindRole: return r.kind;
        case TextRole: return r.text;
        default: return {};
    }
}

QHash<int, QByteArray> EventFeedModel::roleNames() const {
    return {{TimeRole, "time"}, {KindRole, "kind"}, {TextRole, "text"}};
}

void EventFeedModel::add(const QString& kind, const QString& text) {
    if (rows_.size() >= kMaxRows) {
        const int last = static_cast<int>(rows_.size()) - 1;
        beginRemoveRows({}, last, last);
        rows_.removeLast();
        endRemoveRows();
    }
    beginInsertRows({}, 0, 0);
    rows_.prepend({QDateTime::currentDateTimeUtc().toString("HH:mm:ss"), kind, text});
    endInsertRows();
}
