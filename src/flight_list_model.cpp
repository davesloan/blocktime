// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_list_model.h"

#include <memory>
#include <utility>

#include <QDateTime>
#include <QGeoCoordinate>
#include <QList>

namespace {

QDateTime parseUtc(const std::string& iso) {
    if (iso.empty()) return {};
    return QDateTime::fromString(QString::fromStdString(iso), Qt::ISODateWithMs).toUTC();
}

QString hhmmZ(const QDateTime& t) { return t.isValid() ? t.toString(QStringLiteral("HH:mm")) + u'Z' : QString(); }

QString hMm(qint64 seconds) {
    const qint64 minutes = seconds / 60;
    return QStringLiteral("%1:%2").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

FlightListModel::FlightListModel(std::filesystem::path logDir, QObject* parent)
    : QAbstractListModel(parent), logDir_(std::move(logDir)) {}

FlightListModel::~FlightListModel() {
    // ~jthread requests a stop and joins; loadFlightHistory returns as soon as it sees the stop. A result already
    // posted is dropped with this object.
    worker_ = {};
}

void FlightListModel::reload() {
    if (loading_) {
        reloadAgain_ = true;
        return;
    }
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
                apply(std::move(*result));
                setLoading(false);
                if (reloadAgain_) {
                    reloadAgain_ = false;
                    reload();
                }
            },
            Qt::QueuedConnection);
    });
}

void FlightListModel::reloadBlocking() { apply(blocktime::loadFlightHistory(logDir_)); }

void FlightListModel::apply(std::vector<blocktime::FlightRecord> flights) {
    const int before = count();
    beginResetModel();
    flights_ = std::move(flights);
    endResetModel();
    if (count() != before) emit countChanged();
}

void FlightListModel::setLoading(bool loading) {
    if (loading == loading_) return;
    loading_ = loading;
    emit loadingChanged();
}

int FlightListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : count(); }

QVariant FlightListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    const blocktime::FlightRecord& f = flights_[static_cast<std::size_t>(index.row())];
    const QDateTime out = parseUtc(f.outTime);
    const QDateTime end = parseUtc(f.endTime);
    using blocktime::FlightEnd;

    switch (role) {
    case FlightIdRole: return QString::fromStdString(f.flightId);
    // toString(format) uses the C locale, so month names are English whatever Windows is set to.
    case DateTextRole: return out.isValid() ? out.toString(QStringLiteral("d MMM yyyy")) : QString();
    case OutTextRole: return hhmmZ(out);
    case InTextRole: return hhmmZ(end);
    case AtcIdRole: return QString::fromStdString(f.atcId);
    case TitleRole: return QString::fromStdString(f.title);
    case BlockTextRole:
        if (f.blockTimeS) return hMm(*f.blockTimeS);
        if (out.isValid() && end.isValid()) return hMm(out.secsTo(end));
        return QStringLiteral("—");
    case FuelUsedLbRole: return f.inFuelLb ? f.outFuelLb - *f.inFuelLb : -1.0;
    case DistanceNmRole: return f.distanceNm;
    case MaxAltFtRole: return f.maxAltFt;
    case StatusRole:
        return f.end == FlightEnd::BlockIn ? QStringLiteral("block_in")
               : f.end == FlightEnd::Interrupted ? QStringLiteral("interrupted")
                                                 : QStringLiteral("open");
    case StatusTextRole:
        if (f.end == FlightEnd::BlockIn) return QStringLiteral("Block in");
        if (f.end == FlightEnd::Open) return QStringLiteral("In progress");
        return QStringLiteral("Interrupted · ") + QString::fromStdString(f.endReason);
    case EndReasonRole: return QString::fromStdString(f.endReason);
    default: return {};
    }
}

QHash<int, QByteArray> FlightListModel::roleNames() const {
    return {{FlightIdRole, "flightId"},     {DateTextRole, "dateText"},     {OutTextRole, "outText"},
            {InTextRole, "inText"},         {AtcIdRole, "atcId"},           {TitleRole, "title"},
            {BlockTextRole, "blockText"},   {FuelUsedLbRole, "fuelUsedLb"}, {DistanceNmRole, "distanceNm"},
            {MaxAltFtRole, "maxAltFt"},     {StatusRole, "status"},         {StatusTextRole, "statusText"},
            {EndReasonRole, "endReason"}};
}

QGeoPath FlightListModel::track(int row) const {
    if (row < 0 || row >= count()) return {};
    QList<QGeoCoordinate> points;
    const auto& t = flights_[static_cast<std::size_t>(row)].track;
    points.reserve(static_cast<qsizetype>(t.size()));
    for (const auto& p : t) points.append(QGeoCoordinate(p.lat, p.lon));
    return QGeoPath(points);
}
