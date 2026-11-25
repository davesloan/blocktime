// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief review's HTTPS call to the Claude Messages API (D-156, D-161): raw HTTP from Qt, since there's no
// official C++ SDK. The key comes from settings.json (claude_api_key) and is sent only in the x-api-key header.

#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#include "review_writer.h"

class ClaudeReviewer : public QObject, public ReviewClient {
    Q_OBJECT

public:
    explicit ClaudeReviewer(std::string apiKey, QObject* parent = nullptr);

    bool hasKey() const override { return !apiKey_.empty(); }
    void write(std::string body, std::function<void(int status, QByteArray body, QString networkError)> done) override;

private:
    std::string apiKey_;
    QNetworkAccessManager network_;
};
