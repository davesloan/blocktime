// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aircraft_model.h"

#include <algorithm>
#include <chrono>
#include <format>

#include <QBuffer>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>

#include "airframe_read.h"
#include "airline_queries.h"
#include "leasing.h"
#include "money.h"

using namespace blocktime;

namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }
QString pounds(std::optional<int> lb) { return lb ? q(formatNumber(*lb)) + QStringLiteral(" lb") : QStringLiteral("—"); }
QVariantMap row(const QString& label, const QString& value) { return {{"label", label}, {"value", value}}; }

QString holdName(const std::string& hold) {
    if (hold == "main_deck") return QStringLiteral("Main deck");
    QString name = q(hold);
    name[0] = name[0].toUpper();
    return name;
}

QString seatsText(const std::vector<Cabin>& cabins) {
    if (cabins.empty()) return QStringLiteral("Freighter, no seats");
    int total = 0;
    QStringList parts;
    for (const Cabin& c : cabins) {
        total += c.seats;
        parts.push_back(QStringLiteral("%1 %2").arg(c.seats).arg(q(c.cabinClass)));
    }
    return parts.size() == 1 ? QStringLiteral("%1 seats").arg(total)
                             : parts.join(QStringLiteral(" + ")) + QStringLiteral(" = %1").arg(total);
}

int daysBetween(Date from, Date to) {
    return static_cast<int>((std::chrono::sys_days{to} - std::chrono::sys_days{from}).count());
}

}  // namespace

AircraftModel::AircraftModel(db::Database& db, const Catalog& catalog, ReviewClient& claude,
                             std::function<Date()> today, QObject* parent)
    : QObject(parent), db_(db), catalog_(catalog), claude_(claude), today_(std::move(today)) {}

std::optional<AircraftModel::Detail> AircraftModel::load(const std::string& registration) const {
    Detail d;
    {
        db::Statement s(db_,
                        "SELECT a.airplane_id, COALESCE(a.tail_number, ''), COALESCE(a.free_flight_number, ''), t.code,"
                        " a.airframe_name, p.icao_code, a.simbrief_airframe_id, a.fuel_on_board_lb, a.awaiting_delivery,"
                        " a.leased_on, a.monthly_lease_cents,"
                        " EXISTS (SELECT 1 FROM planned_flight f WHERE f.airplane_id = a.airplane_id),"
                        " (SELECT COUNT(*) FROM flight f WHERE f.airplane_id = a.airplane_id),"
                        " (SELECT COALESCE(SUM(unixepoch(block_in_at) - unixepoch(block_out_at)), 0) FROM flight f"
                        "  WHERE f.airplane_id = a.airplane_id)"
                        " FROM airplane a JOIN aircraft_type t USING (aircraft_type_id)"
                        " JOIN airport p ON p.airport_id = a.location_airport_id"
                        " WHERE a.registration = ? AND a.returned_on IS NULL");
        s.bind(1, registration);
        if (!s.next()) return std::nullopt;
        d.id = s.integer(0);
        d.registration = registration;
        d.tail = s.text(1);
        d.flightNumber = s.text(2);
        d.typeCode = s.text(3);
        d.layoutName = s.text(4);
        d.location = s.text(5);
        d.simbriefId = s.text(6);
        d.fuelLb = static_cast<int>(s.integer(7));
        d.awaitingDelivery = s.integer(8) != 0;
        d.leasedOn = parseDate(s.text(9)).value_or(Date{});
        d.monthlyCents = s.integer(10);
        d.planned = s.integer(11) != 0;
        d.flights = static_cast<int>(s.integer(12));
        d.hours = static_cast<double>(s.integer(13)) / 3600.0;
    }
    d.limits = airplaneLimits(db_, d.id).value_or(AirplaneLimits{});
    {
        db::Statement s(db_, "SELECT base_type, max_passengers, empty_weight_lb, mzfw_lb, mtow_lb, max_fuel_lb,"
                             " max_cargo_lb, read_at FROM airplane_simbrief WHERE airplane_id = ?");
        s.bind(1, d.id);
        if (s.next()) {
            SimbriefReading r;
            r.baseType = s.text(0);
            r.maxPassengers = static_cast<int>(s.integer(1));
            r.emptyWeightLb = static_cast<int>(s.integer(2));
            r.mzfwLb = static_cast<int>(s.integer(3));
            r.mtowLb = static_cast<int>(s.integer(4));
            r.maxFuelLb = static_cast<int>(s.integer(5));
            r.maxCargoLb = static_cast<int>(s.integer(6));
            d.simbrief = r;
            d.readAt = s.text(7);
        }
    }
    {
        db::Statement s(db_, "SELECT cabin_class, seats FROM airplane_cabin WHERE airplane_id = ? ORDER BY rowid");
        s.bind(1, d.id);
        while (s.next()) d.cabins.push_back({s.text(0), static_cast<int>(s.integer(1))});
    }
    {
        db::Statement s(db_, "SELECT hold, max_lb, ratio FROM airplane_hold WHERE airplane_id = ? ORDER BY rowid");
        s.bind(1, d.id);
        while (s.next()) d.holds.push_back({s.text(0), static_cast<int>(s.integer(1)), static_cast<int>(s.integer(2))});
    }
    return d;
}

bool AircraftModel::open(const QString& registration) {
    detail_ = load(registration.toStdString());
    ++attempt_;
    rereadState_ = QStringLiteral("idle");
    rereadError_.clear();
    rereadChoices_.clear();
    rereadLayoutIndex_ = -1;
    emit changed();
    return detail_.has_value();
}

void AircraftModel::reload() {
    if (detail_) detail_ = load(detail_->registration);
    emit changed();
}

std::pair<Date, Date> AircraftModel::leaseMonth() const {
    int k = 0;
    while (leasePeriodStart(detail_->leasedOn, k + 1) <= today_()) ++k;
    return {leasePeriodStart(detail_->leasedOn, k), leasePeriodStart(detail_->leasedOn, k + 1)};
}

QString AircraftModel::registration() const { return detail_ ? q(detail_->registration) : QString(); }
QString AircraftModel::title() const { return detail_ ? q(detail_->layoutName) : QString(); }

QString AircraftModel::subtitle() const {
    if (!detail_) return {};
    QStringList parts{q(detail_->typeCode)};
    if (!detail_->tail.empty()) parts.push_back(QStringLiteral("tail %1").arg(q(detail_->tail)));
    if (!detail_->flightNumber.empty()) parts.push_back(QStringLiteral("flight number %1").arg(q(detail_->flightNumber)));
    return parts.join(QStringLiteral(" · "));
}

QString AircraftModel::statusText() const {
    if (!detail_) return {};
    if (detail_->planned) return QStringLiteral("Flight planned at %1").arg(q(detail_->location));
    if (detail_->awaitingDelivery) return QStringLiteral("Awaiting delivery at %1").arg(q(detail_->location));
    return QStringLiteral("Ready at %1").arg(q(detail_->location));
}

QString AircraftModel::glyph() const {
    if (!detail_) return QStringLiteral("ready");
    return detail_->planned ? QStringLiteral("current") : detail_->awaitingDelivery ? QStringLiteral("delivery") : QStringLiteral("ready");
}

QString AircraftModel::leaseMonthText() const {
    if (!detail_) return {};
    const auto [start, next] = leaseMonth();
    return QStringLiteral("%1 of %2 days").arg(daysBetween(start, today_()) + 1).arg(daysBetween(start, next));
}

double AircraftModel::leaseMonthProgress() const {
    if (!detail_) return 0;
    const auto [start, next] = leaseMonth();
    return std::clamp(static_cast<double>(daysBetween(start, today_()) + 1) / daysBetween(start, next), 0.0, 1.0);
}

QVariantMap AircraftModel::layoutCard() const {
    if (!detail_) return {};
    int total = 0;
    for (const Hold& h : detail_->holds) total += h.maxLb;
    return {{"source", "aircraft files"},
            {"rows", QVariantList{row("Layout", q(detail_->layoutName)), row("Seats", seatsText(detail_->cabins)),
                                  row("Holds", pounds(total))}}};
}

QVariantList AircraftModel::holds() const {
    QVariantList out;
    if (!detail_) return out;
    for (const Hold& h : detail_->holds) {
        out.push_back(QVariantMap{{"name", holdName(h.hold)}, {"lb", q(formatNumber(h.maxLb))}, {"share", h.maxLb}});
    }
    return out;
}

QVariantMap AircraftModel::weightsCard() const {
    if (!detail_) return {};
    const auto& s = detail_->simbrief;
    QString source = QStringLiteral("No screenshot yet");
    if (s) {
        const auto read = parseDate(detail_->readAt.substr(0, 10));
        source = QStringLiteral("SimBrief · read %1").arg(read ? q(formatDayMonthYear(*read)) : q(detail_->readAt));
    }
    const AirplaneLimits& l = detail_->limits;
    return {{"source", source},
            {"rows", QVariantList{
                         row("Empty", pounds(s ? s->emptyWeightLb : std::nullopt)),
                         row("MZFW", pounds(s ? s->mzfwLb : std::nullopt)),
                         row("MTOW", pounds(s ? s->mtowLb : std::nullopt)),
                         row("MLW", pounds(l.mlwLb)),
                         row("Max fuel", pounds(s ? s->maxFuelLb : std::nullopt)),
                         row("Passenger / bag", QStringLiteral("%1 / %2 lb").arg(l.passengerWeightLb).arg(l.bagWeightLb)),
                     }}};
}

QVariantMap AircraftModel::simbriefCard() const {
    if (!detail_) return {};
    const auto& s = detail_->simbrief;
    return {{"source", "SimBrief"},
            {"rows", QVariantList{row("Internal ID", q(detail_->simbriefId)),
                                  row("Base type", s ? q(s->baseType) : QStringLiteral("—")),
                                  row("Min runway", q(formatNumber(detail_->limits.minRunwayFt)) + QStringLiteral(" ft")),
                                  row("Max payload", pounds(detail_->limits.maxPayloadLb))}}};
}

QVariantMap AircraftModel::recordCard() const {
    if (!detail_) return {};
    const auto [start, next] = leaseMonth();
    (void)start;
    return {{"source", ""},
            {"rows", QVariantList{
                         row("Fuel on board", pounds(detail_->fuelLb)),
                         row("Flights · hours", QStringLiteral("%1 · %2 h").arg(detail_->flights).arg(detail_->hours, 0, 'f', 1)),
                         row("Monthly", q(formatMoneyFull(detail_->monthlyCents))),
                         row("Next charge", q(formatDayMonthYear(next))),
                         row("Refund if returned today", q(formatMoneyFull(refundIfReturned(db_, detail_->id, today_())))),
                     }}};
}

bool AircraftModel::canReturn() const { return detail_ && !detail_->planned; }
bool AircraftModel::canReread() const { return detail_ && !detail_->planned; }

QString AircraftModel::blockedText() const {
    return detail_ && detail_->planned
               ? QStringLiteral("%1 has a flight planned. Settle or cancel it to change or return it.").arg(q(detail_->registration))
               : QString();
}

QString AircraftModel::returnText() const {
    if (!detail_) return {};
    const Cents refund = refundIfReturned(db_, detail_->id, today_());
    const QString money =
        refund > 0 ? QStringLiteral("%1 comes back for the unused %2 days")
                         .arg(q(formatMoneyFull(refund)))
                         .arg(unusedLeaseDays(detail_->leasedOn, today_()))
                   : QStringLiteral("nothing comes back");
    return QStringLiteral("The lease ends today and %1. Its slot becomes empty. Its flights stay in your history. "
                          "This can't be undone.")
        .arg(money);
}

void AircraftModel::readImage(const QByteArray& bytes, const QString& mediaType) {
    if (!canReread()) return;
    if (!claude_.hasKey()) {
        rereadState_ = QStringLiteral("failed");
        rereadError_ = QStringLiteral("No claude_api_key in settings.json (README).");
        emit changed();
        return;
    }
    rereadState_ = QStringLiteral("reading");
    rereadError_.clear();
    const int attempt = ++attempt_;
    emit changed();
    claude_.write(buildAirframeReadRequest(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())),
                                           mediaType.toStdString()),
                  [this, attempt](int status, QByteArray body, QString networkError) {
                      if (attempt != attempt_ || !detail_) return;
                      if (status == 0) {
                          rereadState_ = QStringLiteral("failed");
                          rereadError_ = QStringLiteral("Couldn't reach Claude: %1").arg(networkError);
                      } else if (const AirframeReadAnswer a = parseAirframeReadResponse(status, body.toStdString());
                                 a.reading) {
                          reading_ = *a.reading;
                          rereadChoices_.clear();
                          for (const LayoutChoice& c :
                               candidateLayouts(catalog_, reading_.baseType, reading_.maxPassengers.value_or(-1))) {
                              if (c.type->code == detail_->typeCode) rereadChoices_.push_back(c);
                          }
                          rereadLayoutIndex_ = -1;
                          for (std::size_t i = 0; i < rereadChoices_.size(); ++i) {  // the current layout if it fits
                              if (rereadChoices_[i].seatsMatch &&
                                  (rereadLayoutIndex_ < 0 || rereadChoices_[i].layout->name == detail_->layoutName)) {
                                  rereadLayoutIndex_ = static_cast<int>(i);
                              }
                          }
                          rereadState_ = QStringLiteral("read");
                      } else {
                          rereadState_ = QStringLiteral("failed");
                          rereadError_ = q(a.error);
                      }
                      emit changed();
                  });
}

void AircraftModel::readImageFile(const QUrl& file) {
    QFile f(file.toLocalFile());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QString suffix = QFileInfo(f).suffix().toLower();
    readImage(f.readAll(), suffix == "png" ? QStringLiteral("image/png") : QStringLiteral("image/jpeg"));
}

void AircraftModel::readClipboard() {
    auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    const QImage image = app ? QGuiApplication::clipboard()->image() : QImage();
    if (image.isNull()) {
        rereadState_ = QStringLiteral("failed");
        rereadError_ = QStringLiteral("The clipboard has no picture. Copy the screenshot first (Win+Shift+S).");
        emit changed();
        return;
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    readImage(bytes, QStringLiteral("image/png"));
}

void AircraftModel::cancelReread() {
    ++attempt_;
    rereadState_ = QStringLiteral("idle");
    rereadError_.clear();
    emit changed();
}

const CatalogAirframe* AircraftModel::rereadLayout() const {
    if (rereadLayoutIndex_ < 0 || rereadLayoutIndex_ >= static_cast<int>(rereadChoices_.size())) return nullptr;
    return rereadChoices_[static_cast<std::size_t>(rereadLayoutIndex_)].layout;
}

void AircraftModel::setRereadLayoutIndex(int index) {
    if (index < 0 || index >= static_cast<int>(rereadChoices_.size()) ||
        !rereadChoices_[static_cast<std::size_t>(index)].seatsMatch) {
        return;
    }
    rereadLayoutIndex_ = index;
    emit changed();
}

QVariantList AircraftModel::rereadLayouts() const {
    QVariantList out;
    if (rereadChoices_.size() < 2) return out;  // only worth a choice when there is one
    for (const LayoutChoice& c : rereadChoices_) {
        out.push_back(QVariantMap{{"name", q(c.layout->name)}, {"detail", seatsText(c.layout->cabins)}, {"enabled", c.seatsMatch}});
    }
    return out;
}

QVariantList AircraftModel::rereadChanges() const {
    QVariantList out;
    if (rereadState_ != "read" || !detail_) return out;
    for (const RereadChange& c : planReread(db_, catalog_, detail_->id, reading_, rereadLayout()).changes) {
        out.push_back(QVariantMap{{"label", q(c.label)}, {"before", q(c.before)}, {"after", q(c.after)}});
    }
    return out;
}

QString AircraftModel::rereadRefusal() const {
    if (rereadState_ != "read" || !detail_) return {};
    const RereadPlan plan = planReread(db_, catalog_, detail_->id, reading_, rereadLayout());
    if (!plan.refusal.empty()) return q(plan.refusal);
    return plan.changes.empty() ? QStringLiteral("Nothing changed: it matches what BlockTime has.") : QString();
}

bool AircraftModel::saveReread() {
    if (rereadState_ != "read" || !detail_) return false;
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const std::string readAt = std::format("{:%Y-%m-%dT%H:%M:%SZ}", now);
    const std::string refusal = applyReread(db_, catalog_, detail_->id, reading_, rereadLayout(), readAt);
    if (!refusal.empty()) {
        rereadError_ = q(refusal);
        rereadState_ = QStringLiteral("failed");
        emit changed();
        return false;
    }
    rereadState_ = QStringLiteral("idle");
    detail_ = load(detail_->registration);
    emit changed();
    emit saved();
    return true;
}

bool AircraftModel::returnPlane() {
    if (!detail_) return false;
    const std::string error = returnAirplane(db_, detail_->id, today_());
    if (!error.empty()) {
        rereadError_ = q(error);
        emit changed();
        return false;
    }
    detail_.reset();
    emit changed();
    emit returned();
    return true;
}
