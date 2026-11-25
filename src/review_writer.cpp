// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "review_writer.h"

#include <chrono>
#include <format>

#include "airline_queries.h"
#include "debrief_review.h"
#include "debrief_score.h"
#include "debrief_store.h"
#include "flight_events.h"
#include "flight_history.h"

using namespace blocktime;

namespace {

constexpr std::size_t kRecentFlights = 5;

std::string hhmm(const std::string& instant) { return instant.size() >= 16 ? instant.substr(11, 5) + "Z" : instant; }

std::vector<std::string> missedLabels(const FlightMeasurements& m) {
    std::vector<std::string> out;
    for (const CheckOutcome& c : scoreFlight(m).checks)
        if (c.state == CheckState::Missed) out.push_back(c.label);
    return out;
}

}  // namespace

ReviewWriter::ReviewWriter(db::Database& db, std::filesystem::path logDir, ReviewClient& client, QObject* parent)
    : QObject(parent), db_(db), logDir_(std::move(logDir)), client_(client) {}

void ReviewWriter::onSettled(const QString& logFlightId) { start(logFlightId); }

void ReviewWriter::retry(const QString& logFlightId) { start(logFlightId); }

QString ReviewWriter::state(const QString& logFlightId) const {
    const std::string id = logFlightId.toStdString();
    if (!isMeasured(db_, id)) return "none";
    if (hasReview(db_, id)) return "written";
    if (writing_.contains(logFlightId)) return "writing";
    if (errors_.contains(logFlightId)) return "failed";
    if (!client_.hasKey()) return "no_key";
    return "missing";
}

QString ReviewWriter::error(const QString& logFlightId) const {
    const auto it = errors_.find(logFlightId);
    return it == errors_.end() ? QString() : it->second;
}

void ReviewWriter::start(const QString& logFlightId) {
    const std::string id = logFlightId.toStdString();
    if (writing_.contains(logFlightId) || !isMeasured(db_, id) || hasReview(db_, id)) return;
    if (!client_.hasKey()) {
        emit reviewChanged(logFlightId);
        return;
    }

    ReviewInput in;
    in.events = loadFlightEvents(logDir_, id, kFlightHistoryFiles);  // logs are kept 30 days
    if (in.events.empty()) return finish(logFlightId, "The flight's event log is gone");
    const auto measured = loadMeasurements(db_);
    in.measurements = measured.at(id);
    // The header and the earlier flights' misses, newest first (listFlights' order).
    bool earlier = false;
    for (const FlightView& f : listFlights(db_)) {
        if (f.logFlightId == id) {
            in.header = std::format("{} → {} · flight {} · {} ({}) · out {} in {}", f.origin, f.arrival, f.flightNumber,
                                    f.registration, f.typeCode, hhmm(f.blockOutAt), hhmm(f.blockInAt));
            earlier = true;
            continue;
        }
        if (!earlier || in.recentMisses.size() >= kRecentFlights) continue;
        if (const auto it = measured.find(f.logFlightId); it != measured.end()) {
            in.recentMisses.push_back(missedLabels(it->second));
        }
    }

    // The request is built from logged data (some of it SayIntentions'); a line it can't read becomes a reason, not a
    // crash on the UI thread.
    std::string body;
    try {
        body = buildReviewRequest(in);
    } catch (const std::exception&) {
        return finish(logFlightId, "Couldn't build the request from the flight's log");
    }
    writing_.insert(logFlightId);
    errors_.erase(logFlightId);
    emit reviewChanged(logFlightId);
    client_.write(std::move(body), [this, logFlightId](int status, QByteArray body, QString) {
        // A network error's text can quote the URL or the host; say only that it failed.
        if (status == 0) return finish(logFlightId, "Couldn't reach Claude");
        ReviewAnswer a = parseReviewResponse(status, body.toStdString());
        if (!a.review) return finish(logFlightId, QString::fromStdString(a.error));
        a.review->writtenAt =
            std::format("{:%FT%T}Z", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
        std::string error;
        if (!storeReview(db_, logFlightId.toStdString(), *a.review, &error)) {
            return finish(logFlightId, "Couldn't save the review: " + QString::fromStdString(error));
        }
        finish(logFlightId, {});
    });
}

void ReviewWriter::finish(const QString& logFlightId, const QString& error) {
    writing_.erase(logFlightId);
    if (error.isEmpty()) {
        errors_.erase(logFlightId);
    } else {
        errors_[logFlightId] = error;
    }
    emit reviewChanged(logFlightId);
}
