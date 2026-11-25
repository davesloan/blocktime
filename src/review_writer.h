// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Claude's review of a settled flight (D-156, D-161, docs/debrief.md section 5): one automatic try right after the
// flight settles, then only when Dave presses Retry. Nothing retries on its own, so a failing call never spends money
// unseen. The reason a try failed is kept only while the app runs.

#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "review_client.h"
#include "sqlite_db.h"

class ReviewWriter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")

public:
    ReviewWriter(blocktime::db::Database& db, std::filesystem::path logDir, ReviewClient& client,
                 QObject* parent = nullptr);

    // The one automatic try: a flight just settled (at block in, or at startup for one that ended while closed).
    void onSettled(const QString& logFlightId);
    Q_INVOKABLE void retry(const QString& logFlightId);

    // "written", "writing", "failed", "no_key", "missing" (measured, no review, nothing tried in this run) or "none"
    // (not measured: flown before 0.11.0).
    Q_INVOKABLE QString state(const QString& logFlightId) const;
    Q_INVOKABLE QString error(const QString& logFlightId) const;  // why the last try in this run failed

signals:
    void reviewChanged(const QString& logFlightId);

private:
    void start(const QString& logFlightId);
    void finish(const QString& logFlightId, const QString& error);

    blocktime::db::Database& db_;
    std::filesystem::path logDir_;
    ReviewClient& client_;
    std::set<QString> writing_;
    std::map<QString, QString> errors_;
};
