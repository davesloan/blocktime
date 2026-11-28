// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "replay_session.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTimeZone>

#include "event_log.h"
#include "feed_text.h"
#include "telemetry_json.h"

using namespace blocktime;

namespace {

constexpr int kTickMs = 50;  // 10 samples a second at 10×

QString q(const std::string& s) { return QString::fromStdString(s); }

// "25 Nov" for the flight id (its block-out time).
QString dayText(const QString& flightId) {
    const auto t = parseUtc(flightId.toStdString());
    if (!t) return flightId;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t->time_since_epoch()).count();
    return QLocale(QLocale::English).toString(QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).date(), "d MMM");
}

}  // namespace

ReplaySession::ReplaySession(std::filesystem::path logDir, QString flightId, std::optional<PlannedFlightView> view,
                             QString airlineIcao, Rates rates, QString airlineName, QObject* parent)
    : QObject(parent), view_(std::move(view)), flightId_(std::move(flightId)), airlineIcao_(std::move(airlineIcao)),
      airlineName_(std::move(airlineName)),
      rates_(rates) {
    timeline_ = loadReplay(logDir, flightId_.toStdString());
    if (timeline_.empty()) error_ = "The log for " + dayText(flightId_) + " is no longer on this PC";
    if (view_) {
        title_ = airlineIcao_ + q(view_->flightNumber) + " " + q(view_->origin) + QString::fromUtf8(" → ") +
                 q(view_->destination) + QString::fromUtf8(" · ") + dayText(flightId_);
    } else {
        QString atc;
        for (const ReplayItem& i : timeline_.items()) {
            if (i.type == "block_out") atc = q(i.data.value("atc_id", std::string()));
        }
        title_ = (atc.isEmpty() ? flightId_ : atc) + QString::fromUtf8(" · ") + dayText(flightId_);
    }
    timer_.setInterval(kTickMs);
    connect(&timer_, &QTimer::timeout, this, [this] {
        const auto elapsed = std::chrono::milliseconds(sinceTick_.restart());
        tick(elapsed);
    });
    makeBridge();
}

void ReplaySession::makeBridge() {
    if (bridge_) bridge_->deleteLater();
    if (header_) header_->deleteLater();
    if (preflight_) preflight_->deleteLater();
    if (inflight_) inflight_->deleteLater();
    if (planned_) planned_->deleteLater();
    bridge_ = new RecorderBridge(this);
    bridge_->setReplay();
    bridge_->setClock([this] { return timeline_.timeAt(feeding_ ? *feeding_ : clock_.offset()); });
    planned_ = new ReplayPlannedFlight(view_, *bridge_, flightId_, this);
    header_ = new DashboardHeader(*bridge_, *planned_, airlineIcao_, airlineName_,
                                  [view = view_](const QString&) { return view; }, this);
    preflight_ = new PreFlightModel(*bridge_, *planned_, this);
    inflight_ =
        new InFlightModel(*bridge_, *planned_, static_cast<int>(rates_.vipFlightAttendantsTimes), header_, this);
}

void ReplaySession::setSpeed(double speed) {
    clock_.setSpeed(speed);
    emit stateChanged();
}

void ReplaySession::setPaused(bool paused) {
    clock_.setPaused(paused);
    updateTimer();
    emit stateChanged();
}

void ReplaySession::restart() {
    clock_.seek(0);
    next_ = 0;
    finished_ = false;
    makeBridge();
    inflight_->beginCatchUp();
    emit restarted();
    feedUpTo(0);
    inflight_->endCatchUp();
    clock_.setPaused(false);
    updateTimer();
    emit stateChanged();
}

void ReplaySession::start() {
    feedUpTo(clock_.offset());
    updateTimer();
}

void ReplaySession::updateTimer() {
    const bool play = !timeline_.empty() && !finished_ && !clock_.paused();
    if (play && !timer_.isActive()) {
        sinceTick_.start();
        timer_.start();
    } else if (!play) {
        timer_.stop();
    }
}

bool ReplaySession::seek(const QString& at) {
    const auto offset = timeline_.offsetOf(at.toStdString());
    if (!offset) return false;
    clock_.seek(*offset);
    inflight_->beginCatchUp();  // a burst: the panel rebuilds without flips (D-198)
    feedUpTo(*offset);
    inflight_->endCatchUp();
    return true;
}

void ReplaySession::tick(std::chrono::milliseconds wall) {
    if (finished_) return;
    feedUpTo(clock_.advance(wall));
}

void ReplaySession::feedUpTo(std::int64_t offsetMs) {
    const auto& items = timeline_.items();
    while (!finished_ && next_ < items.size() && items[next_].offsetMs <= offsetMs) {
        const ReplayItem& item = items[next_++];
        feeding_ = item.offsetMs;
        feed(item);
        feeding_.reset();
        if (item.type == "block_in") {
            // What the recorder logged in the same instant comes with it: phase_changed arrived (milestone 12).
            while (next_ < items.size() && items[next_].offsetMs == item.offsetMs) {
                const ReplayItem& same = items[next_++];
                feeding_ = same.offsetMs;
                feed(same);
                feeding_.reset();
            }
            finished_ = true;
            updateTimer();
            emit stateChanged();
        }
    }
}

void ReplaySession::feed(const ReplayItem& item) {
    switch (item.route) {
        case ReplayRoute::Sample:
            bridge_->applySample(item.sample);
            break;
        case ReplayRoute::Block:
            // As the recorder does: the feed line, then the event.
            if (const auto e = blockEventFromJson(item.type, item.data)) {
                bridge_->applyLine(q(blockFeedText(*e)), false);
                bridge_->applyBlockEvent(*e);
            }
            break;
        case ReplayRoute::Flight: {
            const QVariantMap data =
                QJsonDocument::fromJson(QByteArray::fromStdString(item.data.dump())).object().toVariantMap();
            bridge_->applyFlightEvent(q(item.type), data);
            if (const std::string text = feedText(item.type, item.data); !text.empty()) bridge_->applyLine(q(text), false);
            break;
        }
        case ReplayRoute::Skip:
            break;
    }
}
