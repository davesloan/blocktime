// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The planning step's outside world (design.md §13.2): SayIntentions' runways, SimBrief's latest OFP, and opening the
// dispatch page in the browser. An interface so the models can be tested with a fake; QtPlanner is the real one.

#pragma once

#include <functional>

#include <QByteArray>
#include <QString>

#include "sayintentions.h"

class Planner {
public:
    virtual ~Planner() = default;
    // getWX for both airports. `done` is always called once: on any failure with empty runways and a note.
    virtual void fetchRunways(QString origin, QString destination,
                              std::function<void(blocktime::AtisRunways, QString note)> done) = 0;
    // The latest OFP, fetched only because Dave clicked Import (D-101). `done` gets the body, even on HTTP 400 (which
    // carries SimBrief's status), or a network error when there's no body.
    virtual void fetchLatestOfp(std::function<void(QByteArray body, QString networkError)> done) = 0;
    virtual void openUrl(const QString& url) = 0;
    // During the flight (D-123, FlightWatcher): one airport's getWX, and getCommsHistory. `done` is always called
    // once; `error` is set (and nothing should be logged) for a network failure, an HTTP error or SayIntentions'
    // own {"error": ...}.
    virtual void fetchWx(QString icao, std::function<void(QByteArray body, QString error)> done) = 0;
    virtual void fetchCommsHistory(std::function<void(QByteArray body, QString error)> done) = 0;
    // The arrival gate (milestone 9, D-141): SayIntentions' local flightJSON (no key), assignGate, getParking, and
    // getAirport (--probe only). Same `done` rules as above.
    virtual void fetchSiFlight(std::function<void(QByteArray body, QString error)> done) = 0;
    virtual void assignGate(QString gate, QString icao, std::function<void(QByteArray body, QString error)> done) = 0;
    virtual void fetchParking(std::function<void(QByteArray body, QString error)> done) = 0;
    virtual void fetchSiAirport(std::function<void(QByteArray body, QString error)> done) = 0;
};
