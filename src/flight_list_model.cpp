// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_list_model.h"

#include <cmath>
#include <memory>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QGeoCoordinate>
#include <QList>

#include "debrief_score.h"
#include "loads.h"
#include "money.h"
#include "new_flight_model.h"

namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }

QDateTime parseUtc(const std::string& iso) {
    if (iso.empty()) return {};
    return QDateTime::fromString(q(iso), Qt::ISODate).toUTC();
}

QString hhmmZ(const QDateTime& t) { return t.isValid() ? t.toString(QStringLiteral("HH:mm")) + u'Z' : QString(); }

QString hMm(long long seconds) {
    const long long minutes = seconds / 60;
    return QStringLiteral("%1:%2").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

QVariantList moneyLines(const std::vector<blocktime::MoneyLine>& lines) {
    QVariantList out;
    for (const blocktime::MoneyLine& m : lines) {
        out << QVariantMap{{"label", q(m.label)}, {"amount", q(blocktime::formatMoneyFull(m.cents))}};
    }
    return out;
}

QString kindLabel(const std::string& code) {
    const auto kind = blocktime::parseKind(code);
    return kind ? NewFlightModel::kindLabel(*kind) : q(code);
}

QString signedPoints(int points) { return QString::fromUtf8("−") + QString::number(-points); }  // "−6"

// "Average score 71 (last 10: 74 ▲)": the arrow when the last 10 differ from all of them, as shown.
QString averageText(const QString& label, std::optional<double> all, std::optional<double> last10, bool oneDecimal) {
    if (!all || !last10) return {};
    const auto shown = [&](double v) { return oneDecimal ? QString::number(v, 'f', 1) : QString::number(std::lround(v)); };
    const QString a = shown(*all), l = shown(*last10);
    QString arrow;
    if (l != a) arrow = *last10 > *all ? QString::fromUtf8(" ▲") : QString::fromUtf8(" ▼");
    return QStringLiteral("%1 %2 (last 10: %3%4)").arg(label, a, l, arrow);
}

}  // namespace

FlightListModel::FlightListModel(blocktime::db::Database& db, std::filesystem::path logDir, QString airlineIcao,
                                 ReviewWriter* reviews, QObject* parent)
    : QAbstractListModel(parent),
      db_(db),
      logDir_(std::move(logDir)),
      airlineIcao_(std::move(airlineIcao)),
      reviews_(reviews) {}

FlightListModel::~FlightListModel() {
    // ~jthread requests a stop and joins; loadFlightHistory returns as soon as it sees the stop. A result already
    // posted is dropped with this object.
    worker_ = {};
}

void FlightListModel::readFlights() {
    const int before = count();
    beginResetModel();
    flights_ = blocktime::listFlights(db_);
    readDebriefs();
    endResetModel();
    if (count() != before) emit countChanged();
    emit debriefChanged();
}

void FlightListModel::refreshDebrief() {
    readDebriefs();
    emit debriefChanged();
}

void FlightListModel::readDebriefs() {
    measurements_ = blocktime::loadMeasurements(db_);
    written_ = blocktime::loadReviews(db_);
    // The averages are computed now, from the measurements, so a changed rule re-scores them too (D-160).
    std::vector<blocktime::ScoredFlight> scored;
    for (const blocktime::FlightView& f : flights_) {
        if (const auto it = measurements_.find(f.logFlightId); it != measurements_.end()) {
            scored.push_back(blocktime::scoreFlight(it->second));
        }
    }
    const blocktime::Averages a = blocktime::averages(scored);
    averageScoreText_ = averageText("Average score", a.score, a.scoreLast10, false);
    averageStarsText_ = a.stars ? averageText("Landings " + q(blocktime::starsText(static_cast<int>(std::lround(*a.stars)))),
                                              a.stars, a.starsLast10, true)
                                : QString();
}

void FlightListModel::reload() {
    if (loading_) {
        reloadAgain_ = true;
        return;
    }
    readFlights();
    setLoading(true);
    // The previous load posted its result as its last step, so this join doesn't wait.
    if (worker_.joinable()) worker_.join();
    worker_ = std::jthread([this, dir = logDir_](std::stop_token stop) {
        auto result = std::make_shared<std::vector<blocktime::FlightRecord>>(
            blocktime::loadFlightHistory(dir, blocktime::kFlightHistoryFiles, stop));
        if (stop.stop_requested()) return;
        QMetaObject::invokeMethod(
            this,
            [this, result] {
                applyTracks(std::move(*result));
                setLoading(false);
                if (reloadAgain_) {
                    reloadAgain_ = false;
                    reload();
                }
            },
            Qt::QueuedConnection);
    });
}

void FlightListModel::reloadBlocking() {
    readFlights();
    applyTracks(blocktime::loadFlightHistory(logDir_));
}

void FlightListModel::applyTracks(std::vector<blocktime::FlightRecord> records) {
    tracks_.clear();
    for (blocktime::FlightRecord& r : records) {
        if (r.end == blocktime::FlightEnd::BlockIn) tracks_[r.flightId] = std::move(r);
    }
    if (count() > 0) emit dataChanged(index(0), index(count() - 1), {DistanceNmRole, MaxAltFtRole});
    emit tracksChanged();
}

void FlightListModel::setLoading(bool loading) {
    if (loading == loading_) return;
    loading_ = loading;
    emit loadingChanged();
}

const blocktime::FlightRecord* FlightListModel::recordOf(const blocktime::FlightView& f) const {
    const auto it = tracks_.find(f.logFlightId);
    return it == tracks_.end() ? nullptr : &it->second;
}

int FlightListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : count(); }

QVariant FlightListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const blocktime::FlightView& f = flights_[static_cast<std::size_t>(index.row())];
    const blocktime::FlightRecord* track = recordOf(f);
    switch (role) {
    case FlightIdRole: return q(f.logFlightId);
    // toString(format) uses the C locale, so month names are English whatever Windows is set to.
    case DateTextRole: return parseUtc(f.blockOutAt).toString(QStringLiteral("d MMM yyyy"));
    case OutTextRole: return hhmmZ(parseUtc(f.blockOutAt));
    case InTextRole: return hhmmZ(parseUtc(f.blockInAt));
    case RouteTextRole:
        return q(f.origin) + QString::fromUtf8("–") + q(f.plannedDestination) +
               (f.arrival != f.plannedDestination ? ", diverted to " + q(f.arrival) : QString());
    case FlightNumberRole: return airlineIcao_ + q(f.flightNumber);
    case RegistrationRole: return q(f.registration);
    case TypeRole: return q(f.typeCode);
    case BlockTextRole: return hMm(f.blockS);
    case FuelUsedLbRole: return f.fuelOutLb - f.fuelInLb;
    case TotalTextRole: return q(blocktime::formatMoneyShort(f.totalCents));
    case TotalCentsRole: return static_cast<qlonglong>(f.totalCents);
    case DistanceNmRole: return track ? track->distanceNm : -1.0;
    case MaxAltFtRole: return track ? track->maxAltFt : -1.0;
    default: return {};
    }
}

QHash<int, QByteArray> FlightListModel::roleNames() const {
    return {{FlightIdRole, "flightId"},         {DateTextRole, "dateText"},         {OutTextRole, "outText"},
            {InTextRole, "inText"},             {RouteTextRole, "routeText"},       {FlightNumberRole, "flightNumber"},
            {RegistrationRole, "registration"}, {TypeRole, "typeCode"},             {BlockTextRole, "blockText"},
            {FuelUsedLbRole, "fuelUsedLb"},     {TotalTextRole, "totalText"},       {TotalCentsRole, "totalCents"},
            {DistanceNmRole, "distanceNm"},     {MaxAltFtRole, "maxAltFt"}};
}

QGeoPath FlightListModel::track(int row) const {
    if (row < 0 || row >= count()) return {};
    const blocktime::FlightView& f = flights_[static_cast<std::size_t>(row)];
    QList<QGeoCoordinate> points;
    if (const blocktime::FlightRecord* r = recordOf(f); r && !r->track.empty()) {
        points.reserve(static_cast<qsizetype>(r->track.size()));
        for (const auto& p : r->track) points.append(QGeoCoordinate(p.lat, p.lon));
    } else {
        points = {QGeoCoordinate(f.originLat, f.originLon), QGeoCoordinate(f.arrivalLat, f.arrivalLon)};
    }
    return QGeoPath(points);
}

QVariantMap FlightListModel::debrief(const blocktime::FlightView& f) const {
    const auto it = measurements_.find(f.logFlightId);
    if (it == measurements_.end()) return {{"measured", false}};
    const blocktime::FlightMeasurements& m = it->second;
    const blocktime::ScoredFlight s = blocktime::scoreFlight(m);
    QVariantList checks;
    for (const blocktime::CheckOutcome& c : s.checks) {
        const char* state = c.state == blocktime::CheckState::Passed   ? "passed"
                            : c.state == blocktime::CheckState::Missed ? "missed"
                                                                       : "not_checked";
        checks.append(QVariantMap{{"label", q(c.label)},
                                  {"detail", q(c.detail)},
                                  {"state", QString::fromLatin1(state)},
                                  {"pointsText", c.points < 0 ? signedPoints(c.points) : QString()}});
    }
    QStringList landing;
    if (const auto& l = m.landing) {
        QStringList parts{QStringLiteral("%1 fpm").arg(l->vsFpm),
                          QStringLiteral("%1.%2 G").arg(l->gHundredths / 100).arg(l->gHundredths % 100, 2, 10, QChar('0')),
                          l->bounces == 0   ? QStringLiteral("no bounce")
                          : l->bounces == 1 ? QStringLiteral("1 bounce")
                                            : QStringLiteral("%1 bounces").arg(l->bounces)};
        if (l->pitchTenths) parts << QString::fromUtf8("pitch %1°").arg(*l->pitchTenths / 10.0, 0, 'f', 1);
        if (l->bankTenths) parts << QString::fromUtf8("bank %1°").arg(std::abs(*l->bankTenths) / 10.0, 0, 'f', 1);
        if (l->vrefKt) {
            const int off = l->iasKt - *l->vrefKt;
            parts << QStringLiteral("VREF %1%2").arg(off >= 0 ? "+" : "").arg(off);
        }
        parts << QStringLiteral("wind %1/%2 (%3 kt cross)")
                     .arg(l->windDirDeg, 3, 10, QChar('0'))
                     .arg(l->windKt)
                     .arg(s.crosswindKt.value_or(0));
        landing << parts.join(QString::fromUtf8(" · "));
        if (l->offCentrelineDm && l->offHeadingTenths) {
            landing << QString::fromUtf8("%1 m off the centreline, %2° off the heading (information)")
                           .arg(*l->offCentrelineDm / 10.0, 0, 'f', 1)
                           .arg(*l->offHeadingTenths / 10.0, 0, 'f', 1);
        }
    }
    const QString id = q(f.logFlightId);
    const auto w = written_.find(f.logFlightId);
    QString state = reviews_ ? reviews_->state(id) : (w != written_.end() ? "written" : "missing");
    QVariantMap review;
    if (w != written_.end()) {
        QStringList workOn;
        for (const std::string& item : w->second.workOn) workOn << q(item);
        review = {{"summary", q(w->second.summary)},
                  {"workOn", workOn},
                  {"landing", q(w->second.landing)},
                  {"atc", q(w->second.atc)}};
        state = "written";
    }
    return {{"measured", true},
            {"scoreText", QString::number(s.score)},
            {"starsText", s.stars ? q(blocktime::starsText(*s.stars)) : QString()},
            {"landingPointsText", s.stars && s.landingPoints < 0 ? signedPoints(s.landingPoints) : QString()},
            {"checks", checks},
            {"landingLines", landing},
            {"reviewState", state},
            {"reviewError", reviews_ ? reviews_->error(id) : QString()},
            {"review", review}};
}

QVariantMap FlightListModel::summary(int row) const {
    if (row < 0 || row >= count()) return {};
    const blocktime::FlightView& f = flights_[static_cast<std::size_t>(row)];
    const QString arrow = QString::fromUtf8(" → ");
    QString title = airlineIcao_ + q(f.flightNumber) + QString::fromUtf8(" · ") + q(f.origin) + arrow + q(f.arrival);
    if (f.arrival != f.plannedDestination) title += " (planned " + q(f.plannedDestination) + ")";
    const QDateTime out = parseUtc(f.blockOutAt);
    return {{"flightId", q(f.logFlightId)},
            {"title", title},
            {"dateText", out.toString(QStringLiteral("d MMM yyyy"))},
            {"outText", hhmmZ(out)},
            {"inText", hhmmZ(parseUtc(f.blockInAt))},
            {"blockText", hMm(f.blockS)},
            {"fuelText", q(blocktime::formatNumber(f.fuelOutLb - f.fuelInLb)) + " lb used"},
            {"routeText", q(f.registration) + QString::fromUtf8(" · ") + q(f.typeCode) + QString::fromUtf8(" · ") +
                              kindLabel(f.kind)},
            {"income", moneyLines(f.income)},
            {"costs", moneyLines(f.costs)},
            {"totalText", q(blocktime::formatMoneyFull(f.totalCents))},
            {"parkedText", f.arrivalParking.empty() ? QString() : "Parked at " + q(f.arrivalParking)},
            {"debrief", debrief(f)}};
}

int FlightListModel::rowOf(const QString& flightId) const {
    for (int i = 0; i < count(); ++i) {
        if (q(flights_[static_cast<std::size_t>(i)].logFlightId) == flightId) return i;
    }
    return -1;
}
