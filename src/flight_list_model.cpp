// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_list_model.h"

#include <memory>
#include <utility>

#include <QDateTime>
#include <QGeoCoordinate>
#include <QList>

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

}  // namespace

FlightListModel::FlightListModel(blocktime::db::Database& db, std::filesystem::path logDir, QString airlineIcao,
                                 QObject* parent)
    : QAbstractListModel(parent), db_(db), logDir_(std::move(logDir)), airlineIcao_(std::move(airlineIcao)) {}

FlightListModel::~FlightListModel() {
    // ~jthread requests a stop and joins; loadFlightHistory returns as soon as it sees the stop. A result already
    // posted is dropped with this object.
    worker_ = {};
}

void FlightListModel::readFlights() {
    const int before = count();
    beginResetModel();
    flights_ = blocktime::listFlights(db_);
    endResetModel();
    if (count() != before) emit countChanged();
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

QVariantMap FlightListModel::summary(int row) const {
    if (row < 0 || row >= count()) return {};
    const blocktime::FlightView& f = flights_[static_cast<std::size_t>(row)];
    const QString arrow = QString::fromUtf8(" → ");
    QString title = airlineIcao_ + q(f.flightNumber) + QString::fromUtf8(" · ") + q(f.origin) + arrow + q(f.arrival);
    if (f.arrival != f.plannedDestination) title += " (planned " + q(f.plannedDestination) + ")";
    const QDateTime out = parseUtc(f.blockOutAt);
    return {{"title", title},
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
            {"parkedText", f.arrivalParking.empty() ? QString() : "Parked at " + q(f.arrivalParking)}};
}

int FlightListModel::rowOf(const QString& flightId) const {
    for (int i = 0; i < count(); ++i) {
        if (q(flights_[static_cast<std::size_t>(i)].logFlightId) == flightId) return i;
    }
    return -1;
}
