// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The real Planner: Qt Network for getWX and the OFP fetch, QDesktopServices for the browser. The keys come from
// settings.json and are never logged (the developer key goes only in the X-Developer-Key header).

#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#include "app_settings.h"
#include "planner.h"

class QtPlanner : public QObject, public Planner {
    Q_OBJECT

public:
    explicit QtPlanner(blocktime::AppSettings settings, QObject* parent = nullptr);

    void fetchRunways(QString origin, QString destination,
                      std::function<void(blocktime::AtisRunways, QString note)> done) override;
    void fetchLatestOfp(std::function<void(QByteArray body, QString networkError)> done) override;
    void openUrl(const QString& url) override;
    void fetchWx(QString icao, std::function<void(QByteArray, QString)> done) override;
    void fetchCommsHistory(std::function<void(QByteArray, QString)> done) override;

private:
    // GET with the developer key in its header; the same failure rules as fetchRunways.
    void getSayIntentions(const std::string& url, std::function<void(QByteArray, QString)> done);

    blocktime::AppSettings settings_;
    QNetworkAccessManager network_;
};
