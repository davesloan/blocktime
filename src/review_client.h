// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief review's HTTPS call (D-161), behind an interface so ReviewWriter can be tested without the network.
// ClaudeReviewer is the real one; blocktime-debrief uses it too.

#pragma once

#include <functional>
#include <string>

#include <QByteArray>
#include <QString>

class ReviewClient {
public:
    virtual ~ReviewClient() = default;
    virtual bool hasKey() const = 0;
    // Posts the request body. `done` is called once: the HTTP status (0 when nothing came back), the body, and a
    // network error ("" when an answer came back).
    virtual void write(std::string body, std::function<void(int status, QByteArray body, QString networkError)> done) = 0;
};
