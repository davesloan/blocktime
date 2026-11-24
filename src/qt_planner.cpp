// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_planner.h"

#include <QDesktopServices>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "simbrief.h"

namespace {

constexpr int kRunwayTimeoutMs = 8'000;  // D-105
constexpr int kOfpTimeoutMs = 15'000;

QNetworkRequest request(const std::string& url, int timeoutMs) {
    QNetworkRequest r(QUrl(QString::fromStdString(url)));
    r.setTransferTimeout(timeoutMs);
    r.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BlockTime/" BLOCKTIME_VERSION " (open source)"));
    return r;
}

}  // namespace

QtPlanner::QtPlanner(blocktime::AppSettings settings, QObject* parent)
    : QObject(parent), settings_(std::move(settings)) {}

void QtPlanner::fetchRunways(QString origin, QString destination,
                             std::function<void(blocktime::AtisRunways, QString)> done) {
    if (settings_.sayIntentionsApiKey.empty() || settings_.sayIntentionsDeveloperKey.empty()) {
        done({}, QStringLiteral("No SayIntentions keys in settings.json"));
        return;
    }
    QNetworkRequest r = request(
        blocktime::getWxUrl(settings_.sayIntentionsApiKey, origin.toStdString(), destination.toStdString()),
        kRunwayTimeoutMs);
    r.setRawHeader("X-Developer-Key", QByteArray::fromStdString(settings_.sayIntentionsDeveloperKey));
    QNetworkReply* reply = network_.get(r);
    connect(reply, &QNetworkReply::finished, this, [reply, origin, destination, done = std::move(done)] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        // The error text can quote the URL, which holds the pilot key: say only that it failed.
        if (body.isEmpty() && reply->error() != QNetworkReply::NoError) {
            done({}, QStringLiteral("SayIntentions didn't answer"));
            return;
        }
        // A wrong key or a server error must not look like an airport without an ATIS (D-105). SayIntentions' error
        // text quotes no key.
        if (const std::string error = blocktime::atisError(body.toStdString()); !error.empty()) {
            done({}, "SayIntentions: " + QString::fromStdString(error));
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 400) {
            done({}, QStringLiteral("SayIntentions answered HTTP %1").arg(status));
            return;
        }
        done(blocktime::parseAtisRunways(body.toStdString(), origin.toStdString(), destination.toStdString()), {});
    });
}

void QtPlanner::fetchLatestOfp(std::function<void(QByteArray, QString)> done) {
    if (settings_.simbriefUserId.empty()) {
        done({}, QStringLiteral("Add simbrief_user_id to settings.json"));
        return;
    }
    QNetworkReply* reply = network_.get(request(blocktime::ofpFetchUrl(settings_.simbriefUserId), kOfpTimeoutMs));
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (body.isEmpty() && reply->error() != QNetworkReply::NoError) {
            done({}, QStringLiteral("Couldn't reach SimBrief: ") + reply->errorString());
            return;
        }
        done(body, {});
    });
}

void QtPlanner::openUrl(const QString& url) { QDesktopServices::openUrl(QUrl(url)); }

void QtPlanner::fetchWx(QString icao, std::function<void(QByteArray, QString)> done) {
    getSayIntentions(blocktime::getWxUrl(settings_.sayIntentionsApiKey, icao.toStdString()), std::move(done));
}

void QtPlanner::fetchCommsHistory(std::function<void(QByteArray, QString)> done) {
    getSayIntentions(blocktime::commsHistoryUrl(settings_.sayIntentionsApiKey), std::move(done));
}

void QtPlanner::getSayIntentions(const std::string& url, std::function<void(QByteArray, QString)> done) {
    if (settings_.sayIntentionsApiKey.empty() || settings_.sayIntentionsDeveloperKey.empty()) {
        done({}, QStringLiteral("No SayIntentions keys in settings.json"));
        return;
    }
    QNetworkRequest r = request(url, kRunwayTimeoutMs);
    r.setRawHeader("X-Developer-Key", QByteArray::fromStdString(settings_.sayIntentionsDeveloperKey));
    QNetworkReply* reply = network_.get(r);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        // The error text can quote the URL, which holds the pilot key: say only that it failed.
        if (body.isEmpty() && reply->error() != QNetworkReply::NoError) {
            done({}, QStringLiteral("SayIntentions didn't answer"));
            return;
        }
        if (const std::string error = blocktime::atisError(body.toStdString()); !error.empty()) {
            done({}, "SayIntentions: " + QString::fromStdString(error));
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 400) {
            done({}, QStringLiteral("SayIntentions answered HTTP %1").arg(status));
            return;
        }
        done(body, {});
    });
}
