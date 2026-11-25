// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "claude_reviewer.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "debrief_review.h"

namespace {

constexpr int kReviewTimeoutMs = 300'000;  // adaptive thinking can take minutes (D-161)

}  // namespace

ClaudeReviewer::ClaudeReviewer(std::string apiKey, QObject* parent) : QObject(parent), apiKey_(std::move(apiKey)) {}

void ClaudeReviewer::write(std::string body, std::function<void(int, QByteArray, QString)> done) {
    QNetworkRequest r(QUrl(QString::fromLatin1(blocktime::kReviewUrl)));
    r.setTransferTimeout(kReviewTimeoutMs);
    r.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    r.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BlockTime/" BLOCKTIME_VERSION " (open source)"));
    r.setRawHeader("x-api-key", QByteArray::fromStdString(apiKey_));
    r.setRawHeader("anthropic-version", blocktime::kAnthropicVersion);
    r.setRawHeader("anthropic-beta", blocktime::kFallbackBeta);
    QNetworkReply* reply = network_.post(r, QByteArray::fromStdString(body));
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray answer = reply->readAll();
        // An HTTP error still has a status and a body (the API's error message); only no answer at all is a network
        // error.
        done(status, answer, status == 0 ? reply->errorString() : QString());
    });
}
